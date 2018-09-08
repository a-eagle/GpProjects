// HGT.cpp : 定义控制台应用程序的入口点。
//

#include <iostream>
#include <stdio.h>
#include <time.h>

#include "utils/XString.h"
#include "utils/Http.h"
#include "db/SqlDriver.h"

char wbuf[1024 * 1024];
int wpos;
char token[128];

SqlConnection *db;
PreparedStatement *stmt, *stmt2;

struct ItemInfo {
	char code[8];
	char day[12];
	int jme;  // 深股通净买额
	int mrje; // 深股通买入金额
	int mcje; // 深港通卖出金额
	int cjje; // 深股通成交金额
};

ItemInfo lists[1000];

#if 0
size_t write_data( void *ptr, size_t size, size_t nmemb, void *stream) {
	memcpy(wbuf + wpos, ptr, size * nmemb);
	wpos += size * nmemb;
	wbuf[wpos] = 0;
	return size * nmemb;
}

curl_slist *create_header() {
	curl_slist *chunk = NULL;
	chunk = curl_slist_append(chunk, "Connection: keep-alive");
	chunk = curl_slist_append(chunk, "Upgrade-Insecure-Requests: 1");
	chunk = curl_slist_append(chunk, "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36");
	chunk = curl_slist_append(chunk, "Accept: */*");
	//chunk = curl_slist_append(chunk, "Accept-Encoding: gzip, deflate, sdch");
	chunk = curl_slist_append(chunk, "Accept-Language: zh-CN,zh;q=0.8");
	return chunk;
}

int get_token() {
	CURL *curl;
	CURLcode res;
	curl_slist *chunk = NULL;
	
	curl = curl_easy_init();
	chunk = create_header();
	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    curl_easy_setopt(curl, CURLOPT_URL, "http://data.eastmoney.com/hsgt/000001.html");
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    wpos = 0;
    wbuf[0] = 0;
    res = curl_easy_perform(curl);
    
	curl_easy_cleanup(curl);
	curl_slist_free_all(chunk);
	if(res != CURLE_OK) {
		fprintf(stderr, "get_token failed: %s\n", curl_easy_strerror(res));
		return 0;
	}
	char *p = strstr(wbuf, "token=");
	if (p == NULL) return 0;
	char *p2 = strchr(p, '&');
	if (p2 == NULL) return 0;
	*p2 = 0;
	strcpy(token, p + 6);
	printf("token={%s} \n", token);
	return 1;
}

int get_hgt_data(const char *code, int rt) {
	static char url[512];
	const static char *sh = "http://dcfm.eastmoney.com/EM_MutiSvcExpandInterface/api/js/get?type=HSGTCJB&token=%s&sty=HGT&js=var%%20kzRreuwN={%%22data%%22:(x),%%22pages%%22:(tp)}&ps=500&p=1&sr=-1&filter=&st=DetailDate&cmd=%s&rt=%d";
	const static char *sz = "http://dcfm.eastmoney.com/EM_MutiSvcExpandInterface/api/js/get?type=HSGTCJB&token=%s&sty=SGT&js=var%%20CcANPgAQ={%%22data%%22:(x),%%22pages%%22:(tp)}&ps=500&p=1&sr=-1&filter=&st=DetailDate&cmd=%s&rt=%d";
	if (code[0] == '6')
		sprintf(url, sh, token, code, rt);
	else
		sprintf(url, sz, token, code, rt);
	
	CURL *curl;
	CURLcode res;
	curl_slist *chunk = NULL;

	curl = curl_easy_init();
	chunk = create_header();
	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    wpos = 0;
    wbuf[0] = 0;
    res = curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	curl_slist_free_all(chunk);
	if(res != CURLE_OK) {
		fprintf(stderr, "get_hgt_data failed: %s\n", curl_easy_strerror(res));
		return 0;
	}
	return 1;
}


int get_hgt(const char *code) {
	int hs = code[0] == '6';
	int res = get_hgt_data(code, 50089685);
	if (res == 0) return 0;
	int idx = 0;
	char *p1 = wbuf;
	char *p = wbuf;
	while (1) {
		ItemInfo *info = &lists[idx];
		p1 = strstr(p, "DetailDate");
		if (p1 == NULL) break;
		p1 += 13;
		memcpy(info->day, p1, 4);
		info->day[4] = p1[5];
		info->day[5] = p1[6];
		info->day[6] = p1[8];
		info->day[7] = p1[9];
		p1 += 15;

		p1 = strstr(p1, "GTJME");
		if (p1 == NULL) break;
		p1 += 7;
		info->jme = atoi(p1);

		p1 = strstr(p1, "GTMRJE");
		if (p1 == NULL) break;
		p1 += 8;
		info->mrje = atoi(p1);

		p1 = strstr(p1, "GTMCJE");
		if (p1 == NULL) break;
		p1 += 8;
		info->mcje = atoi(p1);

		p1 = strstr(p1, "GTCJJE");
		if (p1 == NULL) break;
		p1 += 8;
		info->cjje = atoi(p1);
		p = p1;
		++idx;
	}

	//for (int i = 0; i < idx; ++i) {
	//	printf("%s  %11d  %11d  %11d  %11d \n", lists[i].day, lists[i].jme, lists[i].mrje, lists[i].mcje, lists[i].cjje);
	//}

	if ((wpos - (p - wbuf) > 50) || strstr(p, "pages") == NULL) {
		printf("get_hgt fail: %s idx=%d \n", code, idx);
		return 0;
	}
	printf("Fetch %s -> %d \n", code, idx);
	return save_db(code, idx);
}
#endif
int save_db(const char * code, int num) {
	char lastDay[12] = {0};
	strcpy(lastDay, "20050101");
	stmt2->setString(0, code);
	ResultSet *rs = stmt2->executeQuery();
	if (rs->next()) {
		char *sd = rs->getString(0);
		strcpy(lastDay, sd);
	}
	
	for (int i = num - 1; i >= 0; --i) {
		ItemInfo *info = &lists[i];
		if (strcmp(lastDay, info->day) >= 0)
			continue;
		stmt->setString(0, code);
		stmt->setInt(1, atoi(info->day));
		stmt->setInt(2, info->jme / 10000);
		stmt->setInt(3, info->mrje / 10000);
		stmt->setInt(4, info->mcje / 10000);
		stmt->setInt(5, info->cjje / 10000);

		if (stmt->executeUpdate() == 0) {
			printf("Save db fail: %s \n", info->day);
			return 0;
		}
	}
	return 1;
}

int save_db_new(int day, int num) {
	db->setAutoCommit(false);
	for (int i = 0; i < num; ++i) {
		ItemInfo *info = &lists[i];
		stmt->setString(0, info->code);
		stmt->setInt(1, day);
		stmt->setInt(2, info->jme);
		stmt->setInt(3, info->mrje);
		stmt->setInt(4, info->mcje);
		stmt->setInt(5, info->cjje);
		if (stmt->executeUpdate() == 0) {
			printf("Save db fail: %s \n", info->day);
			db->rollback();
			return 0;
		}
	}
	return db->commit();
}

int init_mysql() {
	db = SqlConnection::open("mysql://localhost:3306/tdx_f10", "root", "root");
	stmt = db->prepareStatement("insert into _hgt (_code, _day, _jme, _mrje, _mcje, _cjje) values (?, ?, ?, ?, ?, ?)");
	
	stmt2 = db->prepareStatement("select _id, _day from _hgt where _code = ? order by _id desc limit 1");
	
	printf("Init mysql %s \n", (stmt != NULL && stmt2 != NULL ? "OK" : "fail"));
	return stmt != NULL && stmt2 != NULL;
}

#if 0
int main_get_all(int argc, char** argv) {
	char scode[8];
	FILE *f = NULL;
	curl_global_init(CURL_GLOBAL_ALL);
	if (init_mysql() == 0) goto _end;
	if (get_token() == 0) goto _end;
	
	f = fopen("gp.txt", "rb");
	if (f == NULL) goto _end;
	
	while (1) {
		int code = 0;
		int rd = fread(&code, sizeof(int), 1, f);
		if (rd != 1) break;
		if (code == 0) break;
		if (code >= 300000 && code < 600000)
			continue;
		sprintf(scode, "%06d", code);
		get_hgt(scode);
	}
	//get_hgt("600036");
	
_end:
	printf("--------------END--------------\n");
	getchar();
	return 0;
}
#endif

#if 0
int get_top10(const char *day) {
	static char urlBuf[256];
	CURL *curl;
	CURLcode res;
	curl_slist *chunk = NULL;

	curl = curl_easy_init();
	chunk = create_header();
	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	sprintf(urlBuf, "http://data.eastmoney.com/hsgt/top10/%s.html", day);
    curl_easy_setopt(curl, CURLOPT_URL, urlBuf);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    wpos = 0;
    wbuf[0] = 0;
    res = curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	curl_slist_free_all(chunk);
	if(res != CURLE_OK) {
		fprintf(stderr, "top10 [%s] failed: %s\n", day, curl_easy_strerror(res));
		return 0;
	}
	return 1;
}
#endif

int get_top10_inet(const char *day) {
	static char urlBuf[256];
	sprintf(urlBuf, "http://data.eastmoney.com/hsgt/top10/%s.html", day);
	wpos = 0;
	wbuf[0] = 0;

	HttpConnection con(urlBuf, "GET");
	bool ok = con.connect();
	int rc = con.getResponseCode();
	int len = con.getContentLength();
	if (!ok || rc != 200) {
		fprintf(stderr, "top10 [%s] failed: net connect fail\n", day);
		return 0;
	}
	while (true) {
		int rlen = con.read(wbuf + wpos, sizeof(wbuf) - wpos);
		if (rlen < 0) {
			fprintf(stderr, "top10 [%s] failed: read data fail\n", day);
			return 0;
		}
		if (rlen == 0) {
			break;
		}
		wpos += rlen;
	}
	wbuf[wpos] = 0;
	return 1;
}

int next_day(int day) {
	time_t t1 = time(NULL);
	struct tm *tm1 = localtime(&t1);
	int today = (tm1->tm_year + 1900) * 10000 + (tm1->tm_mon + 1) * 100 + tm1->tm_mday;
	if (day >= today) return 0;
	
	struct tm tm2 = {0};
	tm2.tm_year = day / 10000 - 1900;
	tm2.tm_mon = day / 100 % 100 - 1;
	tm2.tm_mday = day % 100;
	while (1) {
		++tm2.tm_mday;
		time_t t2 = mktime(&tm2);
		struct tm *tm3 = localtime(&t2);
		tm2 = *tm3;
		if (tm2.tm_wday != 0 && tm2.tm_wday != 6)
			break;
	}
	int xd = (tm2.tm_year + 1900) * 10000 + (tm2.tm_mon + 1) * 100 + tm2.tm_mday;
	if (xd <= today) return xd;
	return 0;
}

#define SS(a, b) a = strstr(a, b); if (a == NULL) return 0;
#define BACK_TO(a, c) while (*a != c) --a;

int parse_top10() {
	char *p = wbuf;
	ItemInfo * info = lists;
	for (int i = 0; i < 20; ++i, ++info) {
		if (i == 0 || i == 10) {
			SS(p, "股通十大成交股");
			SS(p, "<tbody>");
		}
		SS(p, "<tr");
		SS(p, "</a></td>");
		p -= 6;
		strncpy(info->code, p, 6);
		SS(p, "</span></td>"); p +=10;
		SS(p, "</span></td>"); p +=10;
		char *p2, *p3;
		
		SS(p, "</span></td>"); p +=10;
		p2 = p - 10;
		BACK_TO(p2, '>'); ++p2;
		double jme = strtod(p2, &p3);
		if (p3 == p2) return 0;
		if (memcmp(p3, "亿", 2) == 0) jme *= 10000;
		info->jme = (int)jme;
		
		SS(p, "</span></td>"); p +=10;
		p2 = p - 10;
		BACK_TO(p2, '>'); ++p2;
		double mrje = strtod(p2, &p3);
		if (p3 == p2) return 0;
		if (memcmp(p3, "亿", 2) == 0) mrje *= 10000;
		info->mrje = (int)mrje;
		
		SS(p, "</span></td>"); p +=10;
		p2 = p - 10;
		BACK_TO(p2, '>'); ++p2;
		double mcje = strtod(p2, &p3);
		if (p3 == p2) return 0;
		if (memcmp(p3, "亿", 2) == 0) mcje *= 10000;
		info->mcje = (int)mcje;
		
		SS(p, "</span></td>"); p +=10;
		p2 = p - 10;
		BACK_TO(p2, '>'); ++p2;
		double cjje = strtod(p2, &p3);
		if (p3 == p2) return 0;
		if (memcmp(p3, "亿", 2) == 0) cjje *= 10000;
		info->cjje = (int)cjje;
		
		// printf("%s %d %d %d %d \n", info->code, info->jme, info->mrje, info->mcje, info->cjje);
	}
	
	return 1;
}

int parse_top10_v2() {
	char *p1 = wbuf;
	ItemInfo * info = lists;
	p1 = strstr(p1, "var DATA1");
	if (p1 == NULL) {
		return 0;
	}
	char *p1e = strstr(p1, "]};");
	if (p1e == NULL) {
		return 0;
	}
	// *p1e = 0;

	char *p2 = strstr(p1e + 1, "var DATA2");
	if (p2 == NULL) {
		return 0;
	}
	char *p2e = strstr(p2, "]};");
	if (p2e == NULL) {
		return 0;
	}
	*p2e = 0;
	
	char *px = p1;
	for (int i = 0; i < 20; ++i, ++info) {
		char *c = strstr(px, "\"Code\":\"");
		if (c == NULL) {
			return 0;
		}
		strncpy(info->code, c + 8, 6);

		c = strstr(px, "GTJME");
		int v = atoi(c + 7);
		info->jme = v / 10000;

		c = strstr(px, "GTMRJE");
		v = atoi(c + 8);
		info->mrje = v / 10000;

		c = strstr(px, "GTMCJE");
		v = atoi(c + 8);
		info->mcje = v / 10000;

		c = strstr(px, "GTCJJE");
		v = atoi(c + 8);
		info->cjje = v / 10000;

		px = c + 10;
	}
	return 1;
}

int main_get_new(int argc, char** argv) {
	// curl_global_init(CURL_GLOBAL_ALL);
	ResultSet *rs;
	int newDay = 30170101;
	char sday[24];
	
	if (init_mysql() == 0) goto _endx;
	rs = db->createStatement()->executeQuery("select max(_day) from _hgt");
	if (rs == NULL) goto _endx;
	if (! rs->next()) goto _endx;
	newDay = rs->getInt(0);
	printf("DB last day: %d \n", newDay);
	
	// use age
	printf("--------------------\n");
	printf("  Select: \n");
	printf("  1: Continue. \n");
	printf("  2 {last day}: Set last day.\n");
	printf("--------------------\n");
	
	if (getchar() == '2') {
		int ld = newDay;
		scanf("%d", &ld);
		--ld;
		if (ld > newDay) newDay = ld;
	}

	while ((newDay = next_day(newDay)) != 0) {
		sprintf(sday, "%d-%02d-%02d", newDay / 10000, newDay / 100 % 100, newDay % 100);
		printf("Fetch %s: ", sday);
		if (get_top10_inet(sday)) {
			if (parse_top10_v2()) {
				if (save_db_new(newDay, 20)) {
					printf("OK \n");
				} else {
					printf("Save DB Fail %s \n", db->getError());
					// break;
				}
			} else {
				printf("Parse Top10 Fail \n");
				break;
			}
		} else {
			printf("Get Top10 Fail \n");
			break;
		}
	}
	
	_endx:
	printf("\n--------------END--------------\n");
	return 0;
}

int main(int argc, char** argv) {
	main_get_new(argc, argv);
	// main_get_all(argc, argv);
	
	/*
	STARTUPINFO si = {0};
	si.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION pi;
	
	CreateProcess("hgt_name/update_hgt_name.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	WaitForSingleObject(pi.hProcess,INFINITE);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	*/
	system("pause");
	return 0;
}




