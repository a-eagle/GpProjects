/* Replace "dll.h" with the name of your header */
#include "dll.h"
#include "comm.h"
#include "hgt.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char DLL_PATH[500];
CRITICAL_SECTION commMutex;

char *GetDllPath() {
	return DLL_PATH;
}

void BeginLock_REF(int len, float* out, float* a, float* b, float* c) {
	EnterCriticalSection(&commMutex);
}

void EndLock_REF(int len, float* out, float* a, float* b, float* c) {
	LeaveCriticalSection(&commMutex);
}

//-----------------------------------------------------------------------------
// 资金
void TdxZJ_REF(int len, float* out, float* ids, float* vals, float *c) {
	// OpenIO();
	int id = (int)ids[0];
	if (id <= 6) {
		int v = (int)vals[0];
		InitZJParam(id, (int)vals[0]);
	} else if (id == 9) {
		InitZJParamDate(vals, len);
	} else if (id == 12) {
		GetZJMax(out, len);
	} else if (id == 13) {
	} else if (id == 20) {
		CalcHgtZJ(out, len);
	} else if (id == 21) {
		CalcHgtZJAbs(out, len);
	}
}

void STRING_REF(int len, float* out, float* code, float* b, float* c) {
	//OpenIO();
	printf("STRING_REF: %x %f %d \n", code, code[len-1], (int)code[0]);
	for (int i = 0; i < len; ++i) out[i] = code[i];
}

//通达信，同行比较_排名
void THBJ_PM_REF(int len, float* out, float* ids, float* code, float* c) {
	// OpenIO();
	GetThbjPM((int)code[len - 1], out, len);
}

//------------------------------------------------------------------------------
PluginTCalcFuncInfo g_CalcFuncSets[] = {
	{1, (pPluginFUNC) & BeginLock_REF},
	{2, (pPluginFUNC) & EndLock_REF},

	{120, (pPluginFUNC) & TdxZJ_REF},
	{122, (pPluginFUNC) & STRING_REF},
	{125, (pPluginFUNC) & THBJ_PM_REF},

	{0, NULL},
};

DLLIMPORT int RegisterTdxFunc(PluginTCalcFuncInfo** pFun) {
	if (*pFun == NULL) {
		(*pFun) = g_CalcFuncSets;
		InitHolidays();
		InitializeCriticalSection(&commMutex);
		return TRUE;
	}

	return FALSE;
}


//DLLIMPORT void HelloWorld () {}

BOOL APIENTRY DllMain (HINSTANCE hInst     /* Library instance handle. */ ,
                       DWORD reason        /* Reason this function is being called. */ ,
                       LPVOID reserved     /* Not used. */ )
{
	GetModuleFileName(hInst, DLL_PATH, sizeof(DLL_PATH));
	char *p = strrchr(DLL_PATH, '\\');
	if (p != NULL) {
		p[1] = 0;
	}
	
    switch (reason) {
      case DLL_PROCESS_ATTACH:
        break;

      case DLL_PROCESS_DETACH:
        break;
        
      case DLL_THREAD_ATTACH:
        break;

      case DLL_THREAD_DETACH:
        break;
    }
    
    /* Returns TRUE on success, FALSE on failure */
    return TRUE;
}
