#pragma once
#define BOOL int

class ResultSet {
public:
	ResultSet(void *obj);
	~ResultSet();
	int getRowsNum();
	int getFieldsNum();
	BOOL isEOF();
	char* getColumnLabel(int column);
	char* getColumnName(int column);
	
	BOOL next();
	char *getString(int columnIndex);
	int getInt(int columnIndex);
	long long getInt64(int columnIndex);
	double getDouble(int columnIndex);
	
private:
	void *mObj;
	void *mRow;
};

class Statement {
public:
	enum ColType {
		CT_INT, CT_INT64, CT_DOUBLE, CT_STRING
	};
	Statement(void *obj);
	~Statement();
	void setBindCapacity(int paramBufSize, int resultBufSize);
	void setIntParam(int paramIdx, int val);
	void setInt64Param(int paramIdx, long long int val);
	void setDoubleParam(int paramIdx, double val);
	void setStringParam(int paramIdx, const char* val);
	BOOL bindParams();
	int getParamsCount();
	void setResult(int colIdx, ColType ct, int maxLen = 0);
	BOOL bindResult();
	BOOL reset();
	BOOL exec();
	BOOL storeResult();
	BOOL fetch();
	char *getString(int columnIndex);
	int getInt(int columnIndex);
	long long getInt64(int columnIndex);
	double getDouble(int columnIndex);
	int getInsertId();
	int getFieldCount();
	int getRowsCount();
	const char* getError();
	ResultSet* getQueryResultMetaData();
private:
	void *mObj;
	void *mParams;
	void *mResults;
	class Buffer;
	Buffer *mParamBuf;
	Buffer *mResBuf;
};

class Mysql {
public:
	Mysql();
	~Mysql();
	void connect(const char *db);
	BOOL selectDatabase(const char *db);
	void close();
	
	int getAffectedRows();
	int getInsertId();
	const char *getError();
	BOOL setCharset(const char *charsetName);
	
	BOOL exec(const char *sql);
	ResultSet *query(const char *sql);
	Statement *prepare(const char *sql);
	
	void setAutoCommit(BOOL autoMode);
	BOOL commit();
	BOOL rollback();
	
private:
	void *mObj;
};
















