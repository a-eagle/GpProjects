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
#include "utils/Thread.h"
#include <richedit.h>

VWindow *win;
SqlConnection *db;
HWND topWnd, f10Wnd;

enum DB_STATUS {
	DS_FAIL, DS_OK, DS_EXISTS
};

struct THBJ {
	char code[8];
	int day; // YYYYMMDD 截止日期
	// 【盈利能力】
	double jlr; // 净利润(万元)
	int jlr_pm; // 净利润_排名
	double mgsy; // 每股收益 (元)
	int mgsy_pm; // 每股收益_排名
	double mgys; // 每股营收
	int mgys_pm; //每股营收_排名
	bool ok;
};

void FindF10Wnd();

class BtnListener : public VListener {
public:
	virtual bool onEvent(VComponent *evtSource, Msg *msg);
	void download_JYBD(); // 交易必读
	void download_THBJ(); // 同行比较
};

bool BtnListener::onEvent(VComponent *src, Msg *msg) {
	if (msg->mId != Msg::CLICK) {
		return false;
	}
	char *id = src->getNode()->getAttrValue("id");
	if (strcmp(id, "JYBD_btn") == 0) {
		download_JYBD();
	} else if (strcmp(id, "THBJ_btn") == 0) {
		download_THBJ();
	}
}

char *GetF10Content() {
	static char buf[1024 * 1024];
	buf[0] = 0;
	SendMessage((HWND)f10Wnd, EM_SETSEL, (WPARAM)0,(WPARAM)-1);
	Sleep(200);
	INPUT in[4];
	memset(in, 0, sizeof(in));
	in[0].type = in[1].type = in[2].type = in[3].type = INPUT_KEYBOARD;
	in[0].ki.wVk = in[2].ki.wVk = VK_CONTROL;
	in[1].ki.wVk = in[3].ki.wVk = 'C';
	in[2].ki.dwFlags = in[3].ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(4, in, sizeof(INPUT));
	Sleep(500);

	while (! OpenClipboard(NULL)) {
		Sleep(100);
		printf("Try open clipboard \n");
	}
	HGLOBAL hg = GetClipboardData(CF_TEXT);
	char *str = (char *)GlobalLock(hg);
	if (str != NULL) {
		strcpy(buf, str);
	}
	GlobalUnlock(hg);
	CloseClipboard();
	return buf;
}

void NextF10() {
	RECT r = {0};
	GetWindowRect(f10Wnd, &r);
	SetCursorPos(r.right - 50, r.top - 20);
	INPUT in[2];
	memset(in, 0, sizeof(in));
	in[0].type = in[1].type = INPUT_MOUSE;
	in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
	SendInput(2, in, sizeof(INPUT));
	Sleep(500);
}

bool ParseF10_JBXX(char *txt, char *code, char *name, char *hy) {
	*code = 0;
	const char *s1 = "——";
	char *p = strstr(txt, s1);
	if (p == NULL) {
		return false;
	}
	p += strlen(s1);
	const char *s2 = "（";
	char *p2 = strstr(p, s2);
	if (p2 == NULL) {
		return false;
	}
	*p2 = 0;
	strcpy(name, p);
	p2 += strlen(s2);
	p = p2;
	for (int i = 0; i < 6; ++i) {
		if (p[i] < '0' || p[i] > '9') {
			return false;
		}
	}
	memcpy(code, p, 6);
	code[6] = 0;
	const char *s3 = "公司所属行业：";
	p = strstr(p, s3);
	if (p == NULL) {
		return false;
	}
	p += strlen(s3);
	p2 = p;
	while (*p2 != 0 && *p2 != '\r' && *p2 != '\n') {
		++p2;
	}
	if (*p2 == 0) {
		return false;
	}
	*p2 = 0;
	--p2;
	while (*p2 == ' ' || *p2 == '\t') {
		*p2 = 0;
		--p2;
	}
	if (strlen(p) > 80) {
		return false;
	}
	strcpy(hy, p);
	return true;
}

DB_STATUS Save_JBXX(char *code, char *name, char *hy) {
	static PreparedStatement * stmt = NULL, *findStmt = NULL;
	if (stmt == NULL) {
		stmt = db->prepareStatement("insert into _base(_code, _name, _hy) values (?, ?, ?)");
		findStmt = db->prepareStatement("select count(*) from _base where _code = ?");
	}
	if (stmt == NULL || findStmt == NULL) {
		return DS_FAIL;
	}
	findStmt->setString(0, code);
	ResultSet *rs = findStmt->executeQuery();
	if (rs == NULL) {
		return DS_FAIL;
	}
	int num = 0;
	if (rs->next()) {
		num = rs->getInt(0);
	}
	if (num >= 1) {
		return DS_EXISTS;
	}
	delete rs;

	stmt->setString(0, code);
	stmt->setString(1, name);
	stmt->setString(2, hy);
	int ar = stmt->executeUpdate();
	if (ar == 1) {
		return DS_OK;
	}
	return DS_FAIL;
}

bool ParseF10_THBJ_YLNL(char *txt, THBJ *info, char **px) {
	const char *s = "截止日期";
	char *p = strstr(txt, s);
	if (p == NULL) return false;
	p += strlen(s);
	char day[16] = {0};
	memcpy(day, p, 4);
	if (p[4] != '-') return false;
	day[4] = p[5];
	day[5] = p[6];
	if (p[7] != '-') return false;
	day[6] = p[8];
	day[7] = p[9];
	for (int i = 0; i < 8; ++i) {
		if (day[i] < '0' || day[i] > '9')
			return false;
	}
	info->day = atoi(day);

	p = strstr(p, info->code);
	if (p == NULL) return false;

	p = strstr(p, "｜");
	if (p == NULL) return false;
	p += strlen("｜");

	p = strstr(p, "｜");
	if (p == NULL) return false;
	p += strlen("｜");

	char *pe = strstr(p, "｜");
	if (pe == NULL) return false;
	*pe = 0;
	info->jlr = strtod(p, NULL);

	p = pe + strlen("｜");
	pe = strstr(p, "｜");
	if (pe == NULL) return false;
	*pe = 0;
	info->jlr_pm = atoi(p);

	p = pe + strlen("｜");
	pe = strstr(p, "｜");
	if (pe == NULL) return false;
	*pe = 0;
	info->mgsy = strtod(p, NULL);

	p = pe + strlen("｜");
	pe = strstr(p, "｜");
	if (pe == NULL) return false;
	*pe = 0;
	info->mgsy_pm = atoi(p);

	p = pe + strlen("｜");
	pe = strstr(p, "｜");
	if (pe == NULL) return false;
	*pe = 0;
	info->mgys = (float)strtod(p, NULL);

	p = pe + strlen("｜");
	pe = strstr(p, "｜");
	if (pe == NULL) return false;
	*pe = 0;
	info->mgys_pm = atoi(p);

	p = pe + strlen("｜");
	while (*p == ' ' || *p == '\t') ++p;
	if (*p != '\r' && *p != '\n') return false;
	*px = p;
	return true;
}

bool ParseF10_THBJ(char *txt, THBJ *info, THBJ *info2) { 
	info->ok = info2->ok = false;
	char *cc = strstr(txt, "（");
	if (cc == NULL) return false;
	cc += strlen("（");
	for (int i = 0; i < 6; ++i) {
		if (cc[i] < '0' || cc[i] > '9') 
			return false;
	}
	memcpy(info->code, cc, 6);
	info->code[6] = 0;
	strcpy(info2->code, info->code);

	const char *s = "【盈利能力】";
	char *p = strstr(txt, s);
	if (p == NULL) return false;
	p += strlen(s);
	p = strstr(p, s);
	if (p == NULL) return false;
	p += strlen(s);
	p = strstr(p, "截止日期");
	if (p == NULL) return false;

	s = "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
	char *eof = strstr(p, s);
	if (eof != NULL) *eof = 0;

	char *px = NULL;
	bool ok = ParseF10_THBJ_YLNL(p, info, &px);
	info->ok = ok;
	if (! ok) return false;
	ok = ParseF10_THBJ_YLNL(px, info2, &px);
	info2->ok = ok;
	return true;
}

DB_STATUS Save_THBJ(THBJ *info, THBJ *info2) {
	static PreparedStatement * stmt = NULL, *findStmt = NULL;
	if (stmt == NULL) {
		stmt = db->prepareStatement("insert into _thbj(_code, _day, _jrl, _jrl_pm, _mgsy, _mgsy_pm , _mgys, _mgys_pm) values (?, ?, ?, ?, ?, ?, ?, ?)");
		findStmt = db->prepareStatement("select _day from _thbj where _code = ? order by _day desc limit 6");
	}
	if (stmt == NULL || findStmt == NULL) {
		return DS_FAIL;
	}
	findStmt->setString(0, info->code);
	ResultSet *rs = findStmt->executeQuery();
	if (rs == NULL) {
		return DS_FAIL;
	}
	int days[10];
	int dnum = 0;
	memset(days, 0, sizeof(days));
	while (rs->next()) {
		days[dnum++] = rs->getInt(0);
	}
	delete rs;

	THBJ *arr[2] = {info, info2};
	for (int i = 0; i < 2; ++i) {
		if (! arr[i]->ok) continue;
		
		// find in days
		bool finded = false;
		for (int k = 0; k < dnum; ++k) {
			if (arr[i]->day == days[k]) {
				finded = true;
				break;
			}
		}
		if (finded) {
			continue;
		}

		stmt->setString(0, arr[i]->code);
		stmt->setInt(1, arr[i]->day);

		stmt->setDouble(2, arr[i]->jlr);
		stmt->setInt(3, arr[i]->jlr_pm);
		stmt->setDouble(4, arr[i]->mgsy);
		stmt->setInt(5, arr[i]->mgsy_pm);
		stmt->setDouble(6, arr[i]->mgys);
		stmt->setInt(7, arr[i]->mgys_pm);
		int ar = stmt->executeUpdate();
		if (ar != 1) return DS_FAIL;
	}
	return DS_OK;
}

class DownloadJYBD : public Runnable {
public:
	virtual void onRun() {
		static char code[8], code2[8], name[64], hy[256];
		int num = 0, times = 0;
		while (TRUE) {
			if (GetForegroundWindow() != topWnd) {
				printf("[paused]\n");
				break;
			}
			if (times >= 10) {
				break;
			}
			char *f10 = GetF10Content();
			bool ok = ParseF10_JBXX(f10, code, name, hy);
			ok == ok && strcmp(code, code2) != 0;
			if (! ok) {
				++times;
				Sleep(300);
				continue;
			}
			if (atoi(code) < atoi(code2)) {
				printf("Finished\n");
				break;
			}
			times = 0;
			strcpy(code2, code);
			printf("%s %s [%s] ", code, name, hy);
			DB_STATUS ds = Save_JBXX(code, name, hy);
			if (ds == DS_OK) {
				printf(" OK \n");
			} else if (ds == DS_EXISTS) {
				printf(" already exists \n");
			} else {
				printf("  --> Save DB Fail\n");
				break;
			}
			NextF10();
		}
	}
};

class DownloadTHBJ : public Runnable {
public:
	virtual void onRun() {
		static char code[8], code2[8];
		THBJ info = {0}, info2 = {0};
		int num = 0, times = 0;
		while (TRUE) {
			if (GetForegroundWindow() != topWnd) {
				printf("[paused]\n");
				break;
			}
			if (times >= 5) {
				times = 0;
				NextF10();
				continue;
			}
			char *f10 = GetF10Content();
			if (strlen(f10) < 50) {
				if (strstr(f10, "尚未有相关资料!") != NULL) {
					times = 0;
					NextF10();
					continue;
				}
			}
			bool ok = ParseF10_THBJ(f10, &info, &info2);
			strcpy(code, info.code);
			ok == ok && strcmp(code, code2) != 0;
			if (! ok) {
				++times;
				Sleep(300);
				continue;
			}
			if (atoi(code) == 1) {
				printf("Finished\n");
				break;
			}
			times = 0;
			strcpy(code2, code);
			printf("[%s] %d %d", code, info.day, info2.day);
			DB_STATUS ds = Save_THBJ(&info, &info2);
			if (ds == DS_OK) {
				printf(" OK \n");
			} else if (ds == DS_EXISTS) {
				printf(" already exists \n");
			} else {
				printf("  --> Save DB Fail\n");
				break;
			}

			NextF10();
		}
	}
};

void BtnListener::download_JYBD() {
	FindF10Wnd();
	if (f10Wnd == NULL) {
		return;
	}
	SetForegroundWindow(topWnd);
	Sleep(500);
	Thread *t = new Thread(new DownloadJYBD());
	t->start();
}

void BtnListener::download_THBJ() {
	FindF10Wnd();
	if (f10Wnd == NULL) {
		return;
	}
	SetForegroundWindow(topWnd);
	Sleep(500);
	Thread *t = new Thread(new DownloadTHBJ());
	t->start();
}

BOOL CALLBACK MidiChildProc(HWND hwnd, LPARAM lParam) {
	static char title[48];
	HWND *w = (HWND *)lParam;
	memset(title, 0, sizeof(title));
	GetWindowText(hwnd, title, sizeof(title) - 1);
	if (strstr(title, "基本资料-") == title) {
		*w = hwnd;
		return TRUE;
	}
	return FALSE;
}

void FindF10Wnd() {
	f10Wnd = NULL;
	HWND top = FindWindow("TdxW_MainFrame_Class", NULL);
	if (top == NULL) {
		MessageBox(win->getWnd(), "Open Tdx First !", "", MB_OK);
		return;
	}
	topWnd = top;
	HWND midi = FindWindowEx(top, NULL, "MDIClient", NULL);
	HWND jbzl = NULL;
	EnumChildWindows(midi, MidiChildProc, (LPARAM)&jbzl);
	if (jbzl == NULL) {
		MessageBox(win->getWnd(), "Open F10 First !", "", MB_OK);
		return;
	}
	jbzl = FindWindowEx(jbzl, NULL, "AfxFrameOrView42", NULL);
	f10Wnd = FindWindowEx(jbzl, NULL, "RICHEDIT", NULL);
	if (f10Wnd == NULL) {
		MessageBox(win->getWnd(), "Open F10 First !", "", MB_OK);
	}
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	// ---- debug -----
	if (lpCmdLine != NULL && strstr(lpCmdLine, "-debug") != NULL) {
		const char *ind[] = {"skin/*.*", NULL};
		BuildBinFile("DownloadTdxF10.bin", ind);
		AllocConsole();
		freopen("CONOUT$", "wb", stdout);
	} else {
		freopen("DownloadTdxF10.log", "wb", stdout);
	}
	db = SqlConnection::open("mysql://localhost:3306/tdx_f10", "root", "root");
	const char *sql = "create table _base(_id integer primary key auto_increment, _code char(6), _name varchar(48), _hy varchar(128) ) ";
	db->createStatement()->executeUpdate(sql);

	sql = "create table _thbj(_id integer primary key auto_increment, _code char(6), _day int, _jrl double, _jrl_pm int, _mgsy double, _mgsy_pm int, _mgys double, _mgys_pm int) ";
	db->createStatement()->executeUpdate(sql);

	UIFactory::init();
	XBinFile::getInstance()->load("DownloadTdxF10.bin");
	win = (VWindow *) UIFactory::fastBuild("xbin://skin/ui.xml", "main-page", NULL);

	win->findById("JYBD_btn")->setListener(new BtnListener());
	win->findById("THBJ_btn")->setListener(new BtnListener());
	
	win->createWnd();
	win->show();
	win->msgLoop();
	UIFactory::destory(win->getNode());

	return 0;
}