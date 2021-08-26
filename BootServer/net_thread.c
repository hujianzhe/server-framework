#include "global.h"
#include "net_thread.h"

static Thread_t* s_ReactorThreads;
static Reactor_t* s_Reactors;
static size_t s_ReactorCnt;
static size_t s_BootReactorThreadCnt;

#ifdef __cplusplus
extern "C" {
#endif

Reactor_t* ptr_g_ReactorAccept(void) { return s_Reactors + s_ReactorCnt; }
Reactor_t* targetReactor(size_t key) { return &s_Reactors[key % s_ReactorCnt]; }
Reactor_t* selectReactor(void) {
	static Atom32_t num = 0;
	unsigned int i = _xadd32(&num, 1);
	return &s_Reactors[i % s_ReactorCnt];
}

int newNetThreadResource(unsigned int cnt) {
	int i;
	size_t nbytes;
	if (!networkSetupEnv()) {
		return 0;
	}
	s_ReactorCnt = cnt;
	nbytes = (sizeof(Thread_t) + sizeof(Reactor_t)) * (s_ReactorCnt + 1);
	s_Reactors = (Reactor_t*)malloc(nbytes);
	if (!s_Reactors) {
		return 0;
	}
	s_ReactorThreads = (Thread_t*)(s_Reactors + s_ReactorCnt + 1);

	for (i = 0; i < s_ReactorCnt + 1; ++i) {
		if (!reactorInit(s_Reactors + i)) {
			break;
		}
	}
	if (i != s_ReactorCnt + 1) {
		while (i--) {
			reactorDestroy(s_Reactors + i);
		}
		free(s_Reactors);
		return 0;
	}
	return 1;
}

void freeNetThreadResource(void) {
	if (s_Reactors) {
		free(s_Reactors);
		s_Reactors = NULL;
		s_ReactorThreads = NULL;
	}
	networkCleanEnv();
}

static unsigned int THREAD_CALL reactorThreadEntry(void* arg) {
	Reactor_t* reactor = (Reactor_t*)arg;
	NioEv_t e[4096];
	int wait_msec = 1000;
	while (g_Valid) {
		int n = reactorHandle(reactor, e, sizeof(e) / sizeof(e[0]), gmtimeMillisecond(), wait_msec);
		if (n < 0) {
			logErr(&g_Log, "reactorHandle error:%d", errnoGet());
			break;
		}
	}
	return 0;
}

BOOL runNetThreads(void) {
	int i;
	for (i = 0; i < s_ReactorCnt + 1; ++i) {
		if (!threadCreate(s_ReactorThreads + i, reactorThreadEntry, s_Reactors + i)) {
			break;
		}
	}
	s_BootReactorThreadCnt = i;
	return i == s_ReactorCnt + 1;
}

void wakeupNetThreads(void) {
	int i;
	for (i = 0; i < s_BootReactorThreadCnt; ++i) {
		reactorWake(s_Reactors + i);
	}
}

void joinNetThreads(void) {
	while (s_BootReactorThreadCnt) {
		threadJoin(s_ReactorThreads[--s_BootReactorThreadCnt], NULL);
	}
}

#ifdef __cplusplus
}
#endif
