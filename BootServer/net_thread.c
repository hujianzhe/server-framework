#include "global.h"
#include "net_thread.h"

Thread_t* g_ReactorThreads;
Thread_t* g_ReactorAcceptThread;
Reactor_t* g_Reactors;
Reactor_t* g_ReactorAccept;
size_t g_ReactorCnt;

#ifdef __cplusplus
extern "C" {
#endif

int newNetThreadResource(void) {
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

void freeNetThreadResource(void) {
	if (g_Reactors) {
		free(g_Reactors);
		g_Reactors = NULL;
		g_ReactorAccept = NULL;
		g_ReactorThreads = NULL;
		g_ReactorAcceptThread = NULL;
	}
	networkCleanEnv();
}

Reactor_t* selectReactor(size_t key) { return &g_Reactors[key % g_ReactorCnt]; }
Reactor_t* ptr_g_ReactorAccept(void) { return g_ReactorAccept; }


#ifdef __cplusplus
}
#endif