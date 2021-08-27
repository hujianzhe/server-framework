#include "global.h"

int g_MainArgc;
char** g_MainArgv;
void* g_ModulePtr;
volatile int g_Valid = 1;
Log_t g_Log;
Config_t g_Config;
TaskThread_t* g_DefTaskThreadPtr;

#ifdef __cplusplus
extern "C" {
#endif

void g_Invalid(void) { g_Valid = 0; }
Log_t* ptr_g_Log(void) { return &g_Log; }
Config_t* ptr_g_Config(void) { return &g_Config; }
TaskThread_t* ptr_g_DefTaskThread(void) { return g_DefTaskThreadPtr; }

#ifdef __cplusplus
}
#endif
