#include <iostream>
#include <stdio.h>
#include <time.h>

#include "utils/XString.h"
#include "utils/Http.h"
#include "db/SqlDriver.h"

static char wbuf[1024 * 1024];
static char token[128];
extern SqlConnection *db;

struct Item {
	int day;
	int jme;
};

static Item items[1500];
static int items_num;

static Item items2[1500];
static int items_num2;

static int read_url(const char *url) {
	HttpConnection con(url, "GET");
	bool ok = con.connect();
	int rc = con.getResponseCode();
	int len = con.getContentLength();
	if (!ok || rc != 200) {
		fprintf(stderr, "read url failed: net connect fail\n");
		return 0;
	}
	int wpos = 0;
	while (true) {
		int rlen = con.read((char *)wbuf + wpos, sizeof(wbuf) - wpos);
		if (rlen < 0) {
			return 0;
		}
		if (rlen == 0) {
			break;
		}
		wpos += rlen;
	}
	wbuf[wpos] = 0;
	con.close();
	return 1;
}

int get_token() {
	int e = read_url("http://data.eastmoney.com/hsgt/000001.html");
	if (e == 0) {
		printf("read token fail \n");
		return 0;
	}
	char *p = strstr(wbuf, "token=");
	if (p == NULL) return 0;
	char *p2 = strchr(p, '&');
	if (p2 == NULL) return 0;
	*p2 = 0;
	strcpy(token, p + 6);
	// printf("token={%s} \n", token);
	return 1;
}

static int str2int_day(char *day) {
	char dd[12] = {0};
	memcpy(dd, day, 4);
	memcpy(&dd[4], day + 5, 2);
	memcpy(&dd[6], day + 8, 2);
	return atoi(dd);
}

// 沪股通历史数据  market = 1
// 深股通历史数据  market = 3
int get_hgt_history(int market, int num) {
	static char url[1024];
	sprintf(url, "http://dcfm.eastmoney.com/EM_MutiSvcExpandInterface/api/js/get?type=HSGTHIS&token=%s&filter=(MarketType=%d)&js=var%%20hLEmuMFI={%%22data%%22:(x),%%22pages%%22:(tp)}&ps=%d&p=1&sr=-1&st=DetailDate&rt=51397258", token, market, num);
	int e = read_url(url);
	if (e == 0) {
		return 0;
	}
	char *p = wbuf;
	while (p != NULL) {
		p = strstr(p, "DetailDate");
		if (p == NULL) return 1;
		p += strlen("DetailDate");
		while (*p < '0' || *p > '9') ++p;
		char *day = p;
		p[10] = 0;
		p += 11;
		p = strstr(p , "DRZJLR");
		if (p == NULL) return 0;
		while ((*p < '0' || *p > '9') && (*p != '-')) ++p;
		char *jme = p;
		while (*p != ',') ++p;
		*p = 0;
		++p;
		
		int iday = str2int_day(day);
		double jmr_d = strtod(jme, NULL);
		bool ff = jmr_d < 0;
		if (ff) jmr_d = -jmr_d;
		int cel = ceil(jmr_d / 100);
		int ijme = ff ? -cel : cel;

		if (market == 1) {
			items[items_num].day = iday;
			items[items_num].jme = ijme;
			items_num++;
		} else {
			items2[items_num2].day = iday;
			items2[items_num2].jme = ijme;
			items_num2++;
		}
		// printf("%d  %d \n", iday, ijme);
	}
	return 1;
}

int dateDiff(int mindate, int maxdate) {
	int days=0,j,flag;
	const int primeMonth[12]={31,29,31,30,31,30,31,31,30,31,30,31};	

	for(j=mindate / 10000; j < maxdate / 10000; ++j)
		days += 366;

	for (j=1; j < maxdate / 100 % 100; j++)
		days += primeMonth[j-1];

	for (j = 1; j < mindate  / 100 % 100; j++)
		days-=primeMonth[j-1];

	days = days + maxdate % 100 - mindate % 100;
	return days;
}


extern int init_mysql();

int main_history(int argc, char** argv) {
	printf("\n====Get Hgt History=======\n");
	int err = get_token();
	if (err == 0) {
		goto _end;
	}

	init_mysql();
	PreparedStatement  *ps = db->prepareStatement("select max(_day) from _hgt where _code = '999999' ");
	ResultSet *rs = ps->executeQuery();
	int lastDay = 20130101;
	if (rs->next()) {
		lastDay = rs->getInt(0);
	}
	time_t xx = time (NULL);
	tm * lc = localtime (&xx);
	int curDay = (lc->tm_year + 1900) * 10000 + (lc->tm_mon + 1) * 100 + (lc->tm_mday);

	int daysNum = dateDiff(lastDay, curDay);
	printf("cur-day:%d  last-day:%d daysNum:%d \n", curDay, lastDay, daysNum);
	
	err = get_hgt_history(1, daysNum);
	if (err == 0) {
		goto _end;
	}
	err = get_hgt_history(3, daysNum);
	if (err == 0) {
		goto _end;
	}

	ps = db->prepareStatement("insert into _hgt(_code, _day, _jme, _mrje, _mcje, _cjje) values (?, ?, ?, 0, 0, 0) ");
	db->setAutoCommit(false);
	bool ok = false;
	char code[8];

	sprintf(code, "%d", 999999);
	for (int i = items_num - 1; i >= 0; --i) {
		if (items[i].day <= lastDay) {
			continue;
		}
		ps->setString(0, code);
		ps->setInt(1, items[i].day);
		ps->setInt(2, items[i].jme);
		if (ps->executeUpdate() == 0) {
			printf("Save hgt history fail : hgt \n");
			db->rollback();
			goto _end;
		}
	}

	sprintf(code, "%d", 399001);
	for (int i = items_num2 - 1; i >= 0; --i) {
		if (items2[i].day <= lastDay) {
			continue;
		}
		ps->setString(0, code);
		ps->setInt(1, items2[i].day);
		ps->setInt(2, items2[i].jme);
		if (ps->executeUpdate() == 0) {
			printf("Save hgt history fail : sgt \n");
			db->rollback();
			goto _end;
		}
	}

	ok = db->commit();
	if (ok) printf("\nCommit ok\n");
	else printf("\nCommit fail\n");
_end:
	return 0;
}