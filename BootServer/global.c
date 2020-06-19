#include "global.h"

int g_MainArgc;
char** g_MainArgv;
void* g_ModulePtr;
int(*g_ModuleInitFunc)(TaskThread_t*, int, char**);
volatile int g_Valid = 1;
Thread_t* g_ReactorThreads;
Thread_t* g_ReactorAcceptThread;
Reactor_t* g_Reactors;
Reactor_t* g_ReactorAccept;
size_t g_ReactorCnt;
Log_t g_Log;

#ifdef __cplusplus
extern "C" {
#endif

int initGlobalResource(void) {
	size_t nbytes;
	if (!networkSetupEnv())
		return 0;
	//g_ReactorCnt = processorCount();
	g_ReactorCnt = 1;
	nbytes = (sizeof(Thread_t) + sizeof(Reactor_t)) * (g_ReactorCnt + 1);
	g_Reactors = (Reactor_t*)malloc(nbytes);
	if (!g_Reactors)
		return 0;
	g_ReactorAccept = g_Reactors + g_ReactorCnt;
	g_ReactorThreads = (Thread_t*)(g_ReactorAccept + 1);
	g_ReactorAcceptThread = g_ReactorThreads + g_ReactorCnt;
	return 1;
}

void freeGlobalResource(void) {
	if (g_Reactors) {
		free(g_Reactors);
		g_Reactors = NULL;
		g_ReactorAccept = NULL;
		g_ReactorThreads = NULL;
		g_ReactorAcceptThread = NULL;
	}
	networkCleanEnv();
}

void g_Invalid(void) { g_Valid = 0; }
Reactor_t* selectReactor(size_t key) { return &g_Reactors[key % g_ReactorCnt]; }
Reactor_t* ptr_g_ReactorAccept(void) { return g_ReactorAccept; }
Log_t* ptr_g_Log(void) { return &g_Log; }

#ifdef __cplusplus
}
#endif