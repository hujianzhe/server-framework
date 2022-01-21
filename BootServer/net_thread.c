#include "global.h"
#include "net_thread.h"

static Reactor_t* s_Reactors;
static size_t s_ReactorCnt;
static size_t s_BootReactorThreadCnt;

#ifdef __cplusplus
extern "C" {
#endif

Reactor_t* acceptReactor(void) { return s_Reactors + s_ReactorCnt; }
Reactor_t* targetReactor(size_t key) { return &s_Reactors[key % s_ReactorCnt]; }
Reactor_t* selectReactor(void) {
	static Atom32_t num = 0;
	unsigned int i = _xadd32(&num, 1);
	return &s_Reactors[i % s_ReactorCnt];
}

int newNetThreadResource(unsigned int cnt) {
	int i;
	if (!networkSetupEnv()) {
		return 0;
	}
	s_Reactors = (Reactor_t*)malloc(sizeof(Reactor_t) * (cnt + 1));
	if (!s_Reactors) {
		return 0;
	}
	for (i = 0; i < cnt + 1; ++i) {
		if (!reactorInit(s_Reactors + i)) {
			break;
		}
	}
	if (i != cnt + 1) {
		while (i--) {
			reactorDestroy(s_Reactors + i);
		}
		free(s_Reactors);
		s_Reactors = NULL;
		return 0;
	}
	s_ReactorCnt = cnt;
	return 1;
}

void freeNetThreadResource(void) {
	if (s_Reactors) {
		int i;
		for (i = 0; i < s_ReactorCnt + 1; ++i) {
			reactorDestroy(s_Reactors + i);
		}
		free(s_Reactors);
		s_Reactors = NULL;
		s_ReactorCnt = 0;
	}
	networkCleanEnv();
}

static unsigned int THREAD_CALL reactorThreadEntry(void* arg) {
	Reactor_t* reactor;
	int wait_msec;
	NioEv_t* ev;
	const size_t ev_cnt = 4096;

	ev = (NioEv_t*)malloc(sizeof(NioEv_t) * ev_cnt);
	if (!ev) {
		return 0;
	}
	reactor = (Reactor_t*)arg;
	wait_msec = 1000;

	while (ptrBSG()->valid) {
		int n = reactorHandle(reactor, ev, ev_cnt, gmtimeMillisecond(), wait_msec);
		if (n < 0) {
			logErr(ptrBSG()->log, "reactorHandle error:%d", errnoGet());
			break;
		}
	}
	free(ev);
	return 0;
}

BOOL runNetThreads(void) {
	int i;
	for (i = 0; i < s_ReactorCnt + 1; ++i) {
		if (!threadCreate(&s_Reactors[i].m_runthread, reactorThreadEntry, s_Reactors + i)) {
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
		threadJoin(s_Reactors[--s_BootReactorThreadCnt].m_runthread, NULL);
	}
}

#ifdef __cplusplus
}
#endif
