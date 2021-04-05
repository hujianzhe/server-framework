#include "global.h"

int g_MainArgc;
char** g_MainArgv;
void* g_ModulePtr;
int(*g_ModuleInitFunc)(TaskThread_t*, int, char**);
volatile int g_Valid = 1;
Log_t g_Log;
int g_ConnectionNum;

#ifdef __cplusplus
extern "C" {
#endif

void g_Invalid(void) { g_Valid = 0; }
void g_FreeMem(void* p) { free(p); }
Log_t* ptr_g_Log(void) { return &g_Log; }

#ifdef __cplusplus
}
#endif
