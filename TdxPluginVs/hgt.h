#pragma once

void InitZJParam(int id, int val);

void InitZJParamDate(float* days, int len);

void CalcHgtZJ(float *out, int len);

void CalcHgtZJAbs(float *out, int len);

void GetZJMax(float *out, int len);

void GetThbjPM(int code, float *out, int len);

void GetThNum(int code, float *out, int len);

void CalcHgtCJJE(float *out, int len);