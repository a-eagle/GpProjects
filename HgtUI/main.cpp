#include "ui/UIFactory.h"
#include <Windows.h>
#include <CommCtrl.h>
#include "ui/XmlParser.h"
#include <atlimage.h>
#include "ui/VComponent.h"
#include "ui/VExt.h"
#include "ui/XBinFile.h"
#include "db/SqlDriver.h"
#include "utils/XString.h"
#include <iostream>

VWindow *win;
SqlConnection *db;
VTableModel *s_model;
VTable *s_table;

struct ItemData {
	char mCol0[128]; // code 
	char mCol1[128]; // name
	char mCol2[128];
	char mCol3[128];
	char mCol4[128];
	char mCol5[128];
	char mCol6[128];
};

ItemData s_items[4000];
int s_items_num, s_column_count;
char *s_titles[20];

void ReadItem(ResultSet *rs, int idx, SqlType *types) {
	char *p = s_items[idx].mCol0;
	for (int i = 0; i < s_column_count; ++i) {
		char *p2 = p + 128 * i;
		switch (types[i]) {
		case SQL_TYPE_NULL:
			*p2 = 0;
			break;
		case SQL_TYPE_DOUBLE:
			sprintf(p2, "%f", (float)rs->getDouble(i));
			break;
		case SQL_TYPE_FLOAT:
			sprintf(p2, "%f", rs->getFloat(i));
			break;
		case SQL_TYPE_INT:
			sprintf(p2, "%d", rs->getInt(i));
			break;
		case SQL_TYPE_INT64:
			sprintf(p2, "%d", (int)rs->getInt64(i));
			break;
		case SQL_TYPE_CHAR:
			sprintf(p2, "%s", rs->getString(i));
			break;
		}
	}
}

void DoQuery() {
	s_items_num = 0;
	s_column_count = 0;

	VTextArea *area = (VTextArea *)win->findById("area");
	char *sql = area->getText();
	PreparedStatement *ps = db->prepareStatement(sql);
	if (ps == NULL) goto _end;
	
	ResultSetMetaData *meta = ps->getMetaData();
	if (meta == NULL) {
		return;
	}
	SqlType st[20] = {SQL_TYPE_NONE};
	s_column_count = meta->getColumnCount();
	for (int i = 0; i < s_column_count; ++i) {
		s_titles[i] = (char *)XString::dup(meta->getColumnLabel(i), XString::GBK);
		st[i] = meta->getColumnType(i);
	}

	ResultSet *rs = ps->executeQuery();
	if (rs == NULL) goto _end;
	while (rs->next()) {
		ReadItem(rs, s_items_num, st);
		++s_items_num;
	}
	delete rs;
	delete ps;

	_end:
	s_table->notifyModelChanged();
}

void DoExport() {
	FILE *f = fopen("hgt.ebk", "wb");
	if (f == 0) return;
	char cc[12];
	for (int i = 0; i < s_items_num; ++i) {
		char *code = s_items[i].mCol0;
		if (code[0] == '6') {
			sprintf(cc, "1%s\n", code);
		} else {
			sprintf(cc, "0%s\n", code);
		}
		fwrite(cc, strlen(cc), 1, f);
	}
	fclose(f);
}

class BtnListener : public VListener {
public:
	BtnListener(int t) {
		mType = t;
	}
	virtual bool onEvent(VComponent *src, Msg *msg) {
		if (msg->mId == Msg::CLICK) {
			if (mType == 1) { // export
				DoExport();
			} else { // query
				DoQuery();
			}
			return true;
		}
		return false;
	}
	int mType;
};

class OpenTdxListener : public VListener {
public:
	virtual bool onEvent(VComponent *src, Msg *msg) {
		VTable *tab = (VTable *)src;
		int col = 0;
		if (msg->mId == Msg::DBCLICK) {
			int row = tab->findCell(msg->mouse.x, msg->mouse.y, &col);
			if (row >= 0) {
				for (int i = 0; i < s_model->getColumnCount(); ++i) {
					char *hd = s_model->getHeaderData(i)->mText;
					if (strcmp(hd, "_code") == 0) {
						openTdx(s_model->getCellData(row, i)->mText);
						break;
					}
				}
			}
		}
		return false;
	}

	void openTdx(char *code) {
		char inc[20];
		if (code == NULL || strlen(code) == 0) {
			return;
		}
		HWND top = FindWindow("TdxW_MainFrame_Class", NULL);
		if (top == NULL) {
			MessageBox(win->getWnd(), "Open Tdx First !", "", MB_OK);
			return;
		}
		SetForegroundWindow(top);

		INPUT in[20];
		sprintf(inc, "%s", code);
		memset(in, 0, sizeof(in));
		for (int i = 0; i < strlen(inc); ++i) {
			in[i * 2].type = INPUT_KEYBOARD;
			in[i * 2].ki.wVk = code[i];
			
			in[i * 2 + 1].type = INPUT_KEYBOARD;
			in[i * 2 + 1].ki.wVk = code[i];
			in[i * 2 + 1].ki.dwFlags = KEYEVENTF_KEYUP;
		}
		SendInput(strlen(inc) * 2, in, sizeof(INPUT));

		in[0].ki.wVk = VK_RETURN;
		in[1].ki.wVk = VK_RETURN;
		Sleep(300);
		SendInput(2, in, sizeof(INPUT));
		// keybd_event(VK_RETURN, 0, 0, 0);
		// keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
	}
};



class TableModel : public VTableModel {
public:
	virtual int getColumnCount() {
		return s_column_count + 1;
	}

	virtual int getRowCount() {
		return s_items_num;
	}

	virtual int getColumnWidth(int col, int wholeWidth) {
		if (s_column_count == 0) {
			return 0;
		}
		if (col == 0) return 50;
		return (wholeWidth - 50) / s_column_count;
	}

	virtual int getRowHeight(int row) {
		return 25;
	}

	virtual int getHeaderHeight() {
		return 30;
	}

	virtual HeaderData *getHeaderData(int col) {
		static HeaderData hd;
		static char buf[80];
		if (col == 0) {
			sprintf(buf, "Idx");
			hd.mText = buf;
		} else {
			hd.mText = s_titles[col - 1];
		}

		return &hd;
	}

	virtual CellData *getCellData(int row, int col) {
		static CellData cd;
		static char txt[128];
		if (col == 0) {
			sprintf(txt, "%03d", row + 1);
			cd.mText = txt;
		} else {
			char *p = s_items[row].mCol0;
			p += 128 * (col - 1);
			cd.mText = p;
		}
		
		return &cd;
	}
};

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	// ---- debug -----
	// AllocConsole();
	// freopen("CONOUT$", "wb", stdout);

	if (lpCmdLine != NULL && strstr(lpCmdLine, "-debug") != NULL) {
		const char *ind[] = {"skin/*.*", NULL};
		BuildBinFile("HgtUi.bin", ind);
	}

	UIFactory::init();
	XBinFile::getInstance()->load("HgtUi.bin");
	win = (VWindow *) UIFactory::fastBuild("skin/ui.xml", "main-page", NULL);

	win->findById("export_btn")->setListener(new BtnListener(1));
	win->findById("query_btn")->setListener(new BtnListener(2));
	VTextArea *area = (VTextArea *)win->findById("area");
	const char *sql = "select a._code, b._name, count(*) as _num from _hgt a \n"
					  "left join _base b on a._code = b._code \n"
					  "where _day >= 20180601 \n"
					  "group by a._code order by _num desc";
	area->setText(sql);

	s_table = (VTable *)(win->findById("table"));
	s_model = new TableModel();
	s_table->setModel(s_model);
	s_table->setListener(new OpenTdxListener());

	db = SqlConnection::open("mysql://localhost:3306/tdx_f10", "root", "root");
	
	win->createWnd();
	win->show();
	win->msgLoop();
	UIFactory::destory(win->getNode());

	return 0;
}