#include "mysql.h"

int D__main(int argc, char **argv) {
	Mysql db;
	db.connect("tdx_f10");
	Statement *stmt2 = db.prepare("select _day, _jme, _cjje from _hgt where _code = ? order by _day asc");
	if (stmt2) {
		stmt2->setBindCapacity(48, 256);
	}
	stmt2->setStringParam(0, "000001");
	stmt2->bindParams();
	stmt2->setResult(0, Statement::CT_INT);
	stmt2->setResult(1, Statement::CT_INT);
	stmt2->setResult(2, Statement::CT_INT);
	stmt2->bindResult();
	stmt2->exec();
	stmt2->storeResult();
	int rc = stmt2->getRowsCount();
	
	while (stmt2->fetch()) {
		int day = stmt2->getInt(0);
		int jme = stmt2->getInt(1);
		int cjje = stmt2->getInt(2);
		int p;
		++p;
	}

	return 0;
}