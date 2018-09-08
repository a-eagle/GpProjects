#include <windows.h>
#include <stdio.h>
#include "mysql.h"
#include "D:/Program Files/MySQL/MySQL Server 5.1/include/mysql.h"

#pragma comment(lib, "D:/Program Files/MySQL/MySQL Server 5.1/lib/opt/libmysql.lib")
ResultSet::ResultSet(void *obj) :mObj(obj) ,mRow(0){
}

ResultSet::~ResultSet() {
	if (mObj) mysql_free_result((MYSQL_RES*)mObj);
}

int ResultSet::getRowsNum() {
	return (int)mysql_num_rows((MYSQL_RES*)mObj);
}

int ResultSet::getFieldsNum() {
	return (int)mysql_num_fields((MYSQL_RES*)mObj);
}

BOOL ResultSet::isEOF() {
	return mysql_eof((MYSQL_RES*)mObj);
}

char* ResultSet::getColumnLabel(int column) {
	MYSQL_FIELD *field = mysql_fetch_field_direct((MYSQL_RES*)mObj, column);
	return field->name;
}

char* ResultSet::getColumnName(int column) {
	MYSQL_FIELD *field = mysql_fetch_field_direct((MYSQL_RES*)mObj, column);
	return field->org_name;
}

BOOL ResultSet::next() {
	mRow = (void*)mysql_fetch_row((MYSQL_RES*)mObj);
	return mRow != NULL;
}

char *ResultSet::getString(int columnIndex) {
	MYSQL_ROW row = (MYSQL_ROW)mRow;
	return row[columnIndex];
}

int ResultSet::getInt(int columnIndex) {
	MYSQL_ROW row = (MYSQL_ROW)mRow;
	char *dat = row[columnIndex];
	return atoi(dat);
}

long long ResultSet::getInt64(int columnIndex) {
	MYSQL_ROW row = (MYSQL_ROW)mRow;
	char *dat = row[columnIndex];
	return (long long)strtod(dat, 0);
}

double ResultSet::getDouble(int columnIndex) {
	MYSQL_ROW row = (MYSQL_ROW)mRow;
	char *dat = row[columnIndex];
	return strtod(dat, 0);
}
//-----------------------------------------------------------
class Statement::Buffer {
public:
	Buffer(int sz) {
		mLen = 0;
		mCapacity = sz;
		mBuf = 0;
	}
	~Buffer() {
		free(mBuf);
	}
	void createBuf() {
		if (mBuf == 0) mBuf = (char*)malloc(mCapacity);
	}
	char *curBuf() {
		return mBuf + mLen;
	}
	void clear() {
		mLen = 0;
	}
	void inc(int sz) {
		mLen += sz;
	}
	void* append(int v) {
		createBuf();
		char *cur = curBuf();
		memcpy(cur, &v, sizeof(int));
		inc(sizeof(int));
		return cur;
	}
	void* append(long long v) {
		createBuf();
		char *cur = curBuf();
		memcpy(cur, &v, sizeof(long long));
		inc(sizeof(long long));
		return cur;
	}
	void* append(double v) {
		createBuf();
		char *cur = curBuf();
		memcpy(curBuf(), &v, sizeof(double));
		inc(sizeof(double));
		return cur;
	}
	void* append(const char* v) {
		createBuf();
		char *cur = curBuf();
		if (v == NULL) v = "";
		int len = strlen(v) + 1;
		strcpy(cur, v);
		inc(len);
		return cur;
	}
	void* appendLen(int len) {
		createBuf();
		char *cur = curBuf();
		inc(len);
		return cur;
	}

	char *mBuf;
	int mCapacity;
	int mLen;
};
const static int BIND_PARAM_NUM = 30;
Statement::Statement(void *obj) : mObj(obj) {
	const int SZ = sizeof(MYSQL_BIND) * BIND_PARAM_NUM;
	mParams = malloc(SZ);
	memset(mParams, 0, SZ);
	mResults = malloc(SZ);
	memset(mResults, 0, SZ);
	mParamBuf = new Buffer(256);
	mResBuf = new Buffer(512);
}

Statement::~Statement() {
	if (mObj) mysql_stmt_close((MYSQL_STMT*)mObj);
	delete mParamBuf;
	delete mResBuf;
}
void Statement::setBindCapacity(int paramBufSize, int resultBufSize) {
	mParamBuf->mCapacity = paramBufSize;
	mResBuf->mCapacity = resultBufSize;
}
void Statement::setIntParam(int paramIdx, int val) {
	MYSQL_BIND *b = (MYSQL_BIND*)mParams + paramIdx;
	b->buffer_type = MYSQL_TYPE_LONG;
	b->buffer = mParamBuf->append(val);
}
void Statement::setInt64Param(int paramIdx, long long int val) {
	MYSQL_BIND *b = (MYSQL_BIND*)mParams + paramIdx;
	b->buffer_type = MYSQL_TYPE_LONGLONG;
	b->buffer = mParamBuf->append(val);
}
void Statement::setDoubleParam(int paramIdx, double val) {
	MYSQL_BIND *b = (MYSQL_BIND*)mParams + paramIdx;
	b->buffer_type = MYSQL_TYPE_DOUBLE;
	b->buffer = mParamBuf->append(val);
}
void Statement::setStringParam(int paramIdx, const char* val) {
	MYSQL_BIND *b = (MYSQL_BIND*)mParams + paramIdx;
	b->buffer_type = MYSQL_TYPE_STRING;
	b->buffer = mParamBuf->append(val);
	b->buffer_length = val ? strlen(val) : 0;
}
BOOL Statement::bindParams() {
	return mysql_stmt_bind_param((MYSQL_STMT*)mObj, (MYSQL_BIND*)mParams) == 0;
}
int Statement::getParamsCount() {
	return (int)mysql_stmt_param_count((MYSQL_STMT*)mObj);
}
void Statement::setResult(int colIdx, ColType ct, int maxLen) {
	static enum_field_types types[] = {MYSQL_TYPE_LONG, MYSQL_TYPE_LONGLONG, MYSQL_TYPE_DOUBLE, MYSQL_TYPE_VAR_STRING};
	static int lens[] = {sizeof(int), sizeof(long long), sizeof(double), 0};
	MYSQL_BIND *b = (MYSQL_BIND*)mResults + colIdx;
	b->buffer_type = types[ct];
	int blen = ct <= CT_DOUBLE ? lens[ct] : maxLen;
	b->buffer_length = blen;
	b->buffer = mResBuf->appendLen(blen);
}
BOOL Statement::bindResult() {
	return mysql_stmt_bind_result((MYSQL_STMT*)mObj, (MYSQL_BIND*)mResults) == 0;
}
BOOL Statement::reset() {
	const int SZ = sizeof(MYSQL_BIND) * BIND_PARAM_NUM;
	memset(mParams, 0, SZ);
	mParamBuf->clear();
	mResBuf->clear();
	mysql_stmt_free_result((MYSQL_STMT*)mObj);
	return mysql_stmt_reset((MYSQL_STMT*)mObj);
}
BOOL Statement::exec() {
	return mysql_stmt_execute((MYSQL_STMT*)mObj) == 0;
}
BOOL Statement::storeResult() {
	return mysql_stmt_store_result((MYSQL_STMT*)mObj) == 0;
}
BOOL Statement::fetch() {
	return mysql_stmt_fetch((MYSQL_STMT*)mObj) == 0;
}
char *Statement::getString(int columnIndex) {
	static char empty[4] = {0};
	*empty = 0;
	MYSQL_BIND *b = (MYSQL_BIND*)mResults + columnIndex;
	if (b->buffer_type != MYSQL_TYPE_VAR_STRING || b->is_null_value)
		return empty;
	return (char*)b->buffer;
}
int Statement::getInt(int columnIndex) {
	MYSQL_BIND *b = (MYSQL_BIND*)mResults + columnIndex;
	if (b->buffer_type != MYSQL_TYPE_LONG || b->is_null_value)
		return 0;
	return *(int*)(b->buffer);
}
long long Statement::getInt64(int columnIndex) {
	MYSQL_BIND *b = (MYSQL_BIND*)mResults + columnIndex;
	if (b->buffer_type != MYSQL_TYPE_LONGLONG || b->is_null_value)
		return 0;
	return *(long long*)(b->buffer);
}
double Statement::getDouble(int columnIndex) {
	MYSQL_BIND *b = (MYSQL_BIND*)mResults + columnIndex;
	if (b->buffer_type != MYSQL_TYPE_DOUBLE || b->is_null_value)
		return 0;
	return *(double*)(b->buffer);
}
int Statement::getInsertId() {
	return (int)mysql_stmt_insert_id((MYSQL_STMT*)mObj);
}
int Statement::getFieldCount() {
	return (int)mysql_stmt_field_count((MYSQL_STMT*)mObj);
}
int Statement::getRowsCount() {
	return (int)mysql_stmt_num_rows((MYSQL_STMT*)mObj);
}
const char* Statement::getError() {
	return mysql_stmt_error((MYSQL_STMT*)mObj);
}
ResultSet* Statement::getQueryResultMetaData() {
	void *d = mysql_stmt_result_metadata((MYSQL_STMT*)mObj);
	if (d == 0) return 0;
	return new ResultSet(d);
}
// ----------------------------------------------------------
Mysql::Mysql() {
	mObj = malloc(sizeof (MYSQL));
	mysql_init((MYSQL*)mObj);
}

Mysql::~Mysql() {
	mysql_close((MYSQL*)mObj);
	free(mObj);
}

int Mysql::getAffectedRows() {
	return (int)mysql_affected_rows((MYSQL*)mObj);
}

int Mysql::getInsertId() {
	return (int)mysql_insert_id((MYSQL*)mObj);
}

const char* Mysql::getError() {
	return mysql_error((MYSQL*)mObj);
}

BOOL Mysql::setCharset(const char *charsetName) {
	return mysql_set_character_set((MYSQL*)mObj, charsetName) == 0;
}

void Mysql::connect(const char *db) {
	mysql_real_connect((MYSQL*)mObj, "localhost", "root", "root", db, 3306, 0, 0);
}

BOOL Mysql::selectDatabase(const char *db) {
	return mysql_select_db((MYSQL*)mObj, db) == 0;
}

void Mysql::close() {
	mysql_close((MYSQL*)mObj);
}

BOOL Mysql::exec(const char *sql) {
	return mysql_query((MYSQL*)mObj, sql) == 0;
}

ResultSet *Mysql::query(const char *sql) {
	MYSQL_RES *res = NULL;
	int code = mysql_query((MYSQL*)mObj, sql);
	if (code != 0) return NULL;
	res = mysql_store_result((MYSQL*)mObj);
	if (res == NULL) return NULL;
	return new ResultSet((void*)res);
}

Statement *Mysql::prepare(const char *sql) {
	MYSQL_STMT *stmt = mysql_stmt_init((MYSQL*)mObj);
	int code = mysql_stmt_prepare(stmt, sql, sql == 0 ? 0 : strlen(sql));
	if (code != 0) {
		printf("::prepare err: %s\n", getError());
		mysql_stmt_close(stmt); // free stmt ?
		return 0;
	}
	return new Statement(stmt);;
}

void Mysql::setAutoCommit(BOOL autoMode) {
	mysql_autocommit((MYSQL*)mObj, (my_bool)autoMode);
}

BOOL Mysql::commit() {
	return mysql_commit((MYSQL*)mObj);
}

BOOL Mysql::rollback() {
	return mysql_rollback((MYSQL*)mObj);
}












