#ifndef _DLL_H_
#define _DLL_H_

#define DLLIMPORT __declspec (dllexport)

#pragma pack(push,1) 
//函数(数据个数,输出,输入a,输入b,输入c)
typedef void(*pPluginFUNC)(int,float*,float*,float*,float*);

typedef struct tagPluginTCalcFuncInfo {
	unsigned short		nFuncMark;//函数编号
	pPluginFUNC			pCallFunc;//函数地址
} PluginTCalcFuncInfo;

typedef int(*pRegisterPluginFUNC)(PluginTCalcFuncInfo**);  
#pragma pack(pop)


extern "C" {
DLLIMPORT int RegisterTdxFunc(PluginTCalcFuncInfo** pFun);
}


#endif /* _DLL_H_ */
