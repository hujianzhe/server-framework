#include "global.h"
#include "net_thread.h"

static struct Reactor_t** s_Reactors;
static size_t s_ReactorCnt;
static Thread_t* s_reactorThreads;
static size_t s_BootReactorThreadCnt;

int newNetThreadResource(unsigned int cnt) {
	int i;
	if (!networkSetupEnv()) {
		return 0;
	}
	s_Reactors = (struct Reactor_t**)malloc(sizeof(*s_Reactors) * (cnt + 1));
	if (!s_Reactors) {
		return 0;
	}
	for (i = 0; i < cnt + 1; ++i) {
		s_Reactors[i] = reactorCreate();
		if (!s_Reactors[i]) {
			break;
		}
	}
	if (i != cnt + 1) {
		while (i) {
			reactorDestroy(s_Reactors[--i]);
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
			reactorDestroy(s_Reactors[i]);
		}
		free(s_Reactors);
		s_Reactors = NULL;
		s_ReactorCnt = 0;
	}
	networkCleanEnv();
}

static unsigned int reactorThreadEntry(void* arg) {
	struct Reactor_t* reactor;
	NioEv_t* ev;
	int res = 0;
	const size_t ev_cnt = 4096;

	ev = (NioEv_t*)malloc(sizeof(NioEv_t) * ev_cnt);
	if (!ev) {
		return 0;
	}
	reactor = (struct Reactor_t*)arg;

	while (ptrBSG()->valid) {
		res = reactorHandle(reactor, ev, ev_cnt, -1);
		if (res < 0) {
			break;
		}
	}
	free(ev);
	return res < 0 ? -1 : 0;
}

BOOL runNetThreads(void) {
	int i;
	s_reactorThreads = (Thread_t*)malloc(sizeof(*s_reactorThreads) * (s_ReactorCnt + 1));
	if (!s_reactorThreads) {
		return FALSE;
	}
	for (i = 0; i < s_ReactorCnt + 1; ++i) {
		if (!threadCreate(s_reactorThreads + i, reactorThreadEntry, s_Reactors[i])) {
			break;
		}
	}
	s_BootReactorThreadCnt = i;
	return i == s_ReactorCnt + 1;
}

void wakeupNetThreads(void) {
	int i;
	for (i = 0; i < s_ReactorCnt + 1; ++i) {
		reactorWake(s_Reactors[i]);
	}
}

void joinNetThreads(void) {
	if (!s_reactorThreads) {
		return;
	}
	while (s_BootReactorThreadCnt) {
		threadJoin(s_reactorThreads[--s_BootReactorThreadCnt], NULL);
	}
	free(s_reactorThreads);
	s_reactorThreads = NULL;
}

#ifdef __cplusplus
extern "C" {
#endif

struct Reactor_t* acceptReactor(void) { return s_Reactors[s_ReactorCnt]; }
struct Reactor_t* targetReactor(size_t key) { return s_Reactors[key % s_ReactorCnt]; }
struct Reactor_t* selectReactor(void) {
	static Atom32_t num = 0;
	unsigned int i = _xadd32(&num, 1);
	return s_Reactors[i % s_ReactorCnt];
}

#ifdef __cplusplus
}
#endif
