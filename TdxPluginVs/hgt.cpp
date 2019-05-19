/* Replace "dll.h" with the name of your header */
#include "dll.h"
#include <windows.h>
#include <stdio.h>
#include "mysql.h"
#define _MAX(a, b) (a > b ? a : b)
#define _ABS(a) (a > 0 ? a : -a)

Mysql db;
Statement *stmt2;

enum ID_PARAM {
	IP_CODE, IP_CDD, IP_DD, IP_ZD, IP_XD, IP_JME, IP_CJJE, IP_NUM
};

struct HGT {
	int day;
	int jme, cjje;
};

int params[IP_NUM];
int days[2000];
int daysLen;
int zjMax;
HGT hgt[2000];
int hgtLen;

BOOL needQuery;

void InitMysql() {
	if (stmt2 != 0) return;
	db.connect("tdx_f10");
	stmt2 = db.prepare("select _day, _jme, _cjje from _hgt where _code = ?");
	if (stmt2) stmt2->setBindCapacity(48, 256);
}

void InitZJParam(int id, int val) {
	if (id == IP_CODE) {
		needQuery = val != params[id];
		if (needQuery) zjMax = 0;
	}
	params[id] = val;
	//printf("InitZJParam id=%d  val=%d \n", id, val);
}

void InitZJParamDate(float* ds, int len) {
	int DL = sizeof(days)/sizeof(int);
	int ml = len < DL ? len : DL;
	for (int i = 0; i < ml; ++i) {
		days[i] = (int)ds[len - ml + i] + 19000000;
	}
	daysLen = ml;
	//printf("InitZJParamDate date=[%d - %d] len:%d \n", (int)ds[0], (int)ds[len-1], len);
	//printf("     days=[%d - %d] len:%d \n", days[0], days[daysLen-1], daysLen);
}

void QueryResult() {
	InitMysql();
	//printf("QueryResult IN need:%d stmt=%p \n", needQuery, stmt);
	if (! needQuery) return;
	hgtLen = 0;
	if (stmt2 == 0) return;
	int MAX_DL = sizeof(days)/sizeof(int);
	char code[8];
	sprintf(code, "%06d", params[IP_CODE]);
	memset(hgt, 0, sizeof hgt);
	
	stmt2->reset();
	stmt2->setStringParam(0, code);
	stmt2->bindParams();
	stmt2->setResult(0, Statement::CT_INT);
	stmt2->setResult(1, Statement::CT_INT);
	stmt2->setResult(2, Statement::CT_INT);
	stmt2->bindResult();
	stmt2->exec();
	stmt2->storeResult();
	int rc = stmt2->getRowsCount();
	for (int i = 0; i < rc - MAX_DL; ++i) {
		stmt2->fetch();
	}
	while (stmt2->fetch()) {
		HGT *p = &hgt[hgtLen];
		p->day = stmt2->getInt(0);
		p->jme = stmt2->getInt(1);
		p->cjje = stmt2->getInt(2);
		++hgtLen;
	}
}

inline int FindDay(int day, int from) {
	for (int i = from; i < daysLen; ++i) {
		if (days[i] == day)
			return i;
	}
	return -1;
}

void CalcHgtZJ(float *out, int len) {
	QueryResult();
	memset(out, 0, sizeof(float) * len);
	int begin = len - daysLen;
	int from = 0;
	for (int i = 0; i < hgtLen; ++i) {
		HGT *p = &hgt[i];
		int j = FindDay(p->day, from);
		if (j == -1) {
			//out[begin + j] = 0;
			continue;
		}
		from = j + 1;
		out[begin + j] = p->jme;
		//printf("day = %d out[%d]=%d \n", p->day, j+begin, (int)out[begin + j]);
	}
	//for (int i = 0; i < begin; ++i) out[i] = 0;
}

void CalcHgtZJAbs(float *out, int len) {
	CalcHgtZJ(out, len);
	for (int i = 0; i < len; ++i) {
		if (out[i] < 0) out[i] = -out[i];
	}
}

void GetZJMax(float *out, int len) {
	out[len - 1] = zjMax;
}

void GetThbjPM(int code, float *out, int len) {
	static Statement *stmt;
	InitMysql();

	out[len - 1] = out[len - 2] = out[len - 3] = out[len - 4] = 0;
	
	if (stmt == NULL) {
		stmt = db.prepare("select _day, _jrl_pm from _thbj where _code = ? order by _day asc ");
		if (stmt == NULL) {
			return;
		}
		stmt->setBindCapacity(48, 256);
	}
	char scode[8];
	sprintf(scode, "%06d", code);
	stmt->reset();
	stmt->setStringParam(0, scode);
	stmt->bindParams();
	stmt->setResult(0, Statement::CT_INT);
	stmt->setResult(1, Statement::CT_INT);
	stmt->bindResult();
	stmt->exec();
	stmt->storeResult();
	int rc = stmt->getRowsCount();
	out[len - 1] = rc;
	int i = 1;
	while (stmt->fetch()) {
		int pm = stmt->getInt(1);
		out[len - 1 - i] = pm;
		++i;
	}
}

void GetThNum( int code, float *out, int len ) {
	static Statement *stmt1, *stmt2;
	InitMysql();

	out[len - 1] = out[len - 2] = out[len - 3] = out[len - 4] = 0;
	if (stmt1 == NULL) {
		stmt1 = db.prepare("select _hy from _base where _code = ? ");
		if (stmt1 == NULL) {
			return;
		}
		stmt1->setBindCapacity(48, 256);
	}
	if (stmt2 == NULL) {
		stmt2 = db.prepare("select count(*) from _base where _hy = ? ");
		if (stmt2 == NULL) {
			return;
		}
		stmt2->setBindCapacity(256, 256);
	}
	char scode[8];
	sprintf(scode, "%06d", code);
	stmt1->reset();
	stmt2->reset();

	stmt1->setStringParam(0, scode);
	stmt1->bindParams();
	stmt1->setResult(0, Statement::CT_STRING, 125);
	stmt1->bindResult();
	stmt1->exec();
	stmt1->storeResult();

	if (! stmt1->fetch()) {
		printf("GetThNum  B2--Error:%s-\n", stmt1->getError());
		return;
	}

	char *hy = stmt1->getString(0);
	if (hy == NULL || strlen(hy) == 0) {
		return;
	}

	stmt2->setStringParam(0, hy);
	stmt2->bindParams();
	stmt2->setResult(0, Statement::CT_INT);
	stmt2->bindResult();
	stmt2->exec();
	stmt2->storeResult();
	if (! stmt2->fetch()) {
		return;
	}
	int num = stmt2->getInt(0);
	out[len - 1] = num;
}
