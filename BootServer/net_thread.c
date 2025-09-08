#include "global.h"
#include "net_thread.h"

static struct NetReactor_t** s_NetReactors;
static size_t s_NetReactorCnt;
static Thread_t* s_NetThreads;
static size_t s_BootNetThreadCnt;

int newNetThreadResource(unsigned int cnt) {
	int i;
	if (!networkSetupEnv()) {
		return 0;
	}
	s_NetReactors = (struct NetReactor_t**)malloc(sizeof(*s_NetReactors) * (cnt + 1));
	if (!s_NetReactors) {
		return 0;
	}
	for (i = 0; i < cnt + 1; ++i) {
		s_NetReactors[i] = NetReactor_create();
		if (!s_NetReactors[i]) {
			break;
		}
	}
	if (i != cnt + 1) {
		while (i) {
			NetReactor_destroy(s_NetReactors[--i]);
		}
		free(s_NetReactors);
		s_NetReactors = NULL;
		return 0;
	}
	s_NetReactorCnt = cnt;
	return 1;
}

void freeNetThreadResource(void) {
	if (s_NetReactors) {
		int i;
		for (i = 0; i < s_NetReactorCnt + 1; ++i) {
			NetReactor_destroy(s_NetReactors[i]);
		}
		free(s_NetReactors);
		s_NetReactors = NULL;
		s_NetReactorCnt = 0;
	}
	networkCleanEnv();
}

static unsigned int net_thread_entry(void* arg) {
	struct NetReactor_t* reactor;
	NioEv_t* ev;
	int res = 0;
	const size_t ev_cnt = 4096;

	ev = (NioEv_t*)malloc(sizeof(NioEv_t) * ev_cnt);
	if (!ev) {
		return 0;
	}
	reactor = (struct NetReactor_t*)arg;

	while (1) {
		memoryBarrierAcquire();
		if (!ptrBSG()->valid) {
			break;
		}
		res = NetReactor_handle(reactor, ev, ev_cnt, -1);
		if (res < 0) {
			break;
		}
	}
	free(ev);
	return res < 0 ? -1 : 0;
}

BOOL runNetThreads(void) {
	int i;
	s_NetThreads = (Thread_t*)malloc(sizeof(*s_NetThreads) * (s_NetReactorCnt + 1));
	if (!s_NetThreads) {
		return FALSE;
	}
	for (i = 0; i < s_NetReactorCnt + 1; ++i) {
		if (!threadCreate(s_NetThreads + i, 0, net_thread_entry, s_NetReactors[i])) {
			break;
		}
	}
	s_BootNetThreadCnt = i;
	return i == s_NetReactorCnt + 1;
}

void wakeupNetThreads(void) {
	int i;
	for (i = 0; i < s_NetReactorCnt + 1; ++i) {
		NetReactor_wake(s_NetReactors[i]);
	}
}

void joinNetThreads(void) {
	if (!s_NetThreads) {
		return;
	}
	while (s_BootNetThreadCnt) {
		threadJoin(s_NetThreads[--s_BootNetThreadCnt], NULL);
	}
	free(s_NetThreads);
	s_NetThreads = NULL;
}

#ifdef __cplusplus
extern "C" {
#endif

struct NetReactor_t* acceptNetReactor(void) { return s_NetReactors[s_NetReactorCnt]; }
struct NetReactor_t* targetNetReactor(size_t key) { return s_NetReactors[key % s_NetReactorCnt]; }
struct NetReactor_t* selectNetReactor(void) {
	static Atom32_t num = 0;
	unsigned int i = xadd32(&num, 1);
	return s_NetReactors[i % s_NetReactorCnt];
}

#ifdef __cplusplus
}
#endif
