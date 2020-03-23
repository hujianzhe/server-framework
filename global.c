#include "global.h"

volatile int g_Valid = 1;
Thread_t* g_ReactorThreads;
Thread_t* g_ReactorAcceptThread;
Thread_t g_TaskThread;
Reactor_t* g_Reactors;
Reactor_t* g_ReactorAccept;
DataQueue_t g_DataQueue;
Fiber_t* g_DataFiber;
size_t g_ReactorCnt;
RBTimer_t g_Timer;

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
	g_DataFiber = NULL;
	networkCleanEnv();
}

Reactor_t* selectReactor(size_t key) {
	return &g_Reactors[key % g_ReactorCnt];
}
