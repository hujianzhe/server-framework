#include "global.h"

static BootServerGlobal_t s_BSG, *s_PtrBSG;
static Atom32_t s_Run;

#ifdef __cplusplus
extern "C" {
#endif

BootServerGlobal_t* ptrBSG(void) { return s_PtrBSG; }
const char* getBSGErrmsg(void) { return s_BSG.errmsg ? s_BSG.errmsg : ""; }

BOOL initBootServerGlobal(const BootServerConfig_t* conf, TaskThread_t* def_task_thrd, const NetScheHook_t* nsh) {
	if (s_PtrBSG) {
		return TRUE;
	}
	s_BSG.conf = conf;
	if (!nsh) {
		nsh = getNetScheHookStackCo();
	}
	s_BSG.net_sche_hook = nsh;
	/* init log */
	s_BSG.log = logOpen();
	if (!s_BSG.log) {
		s_BSG.errmsg = strFormat(NULL, "logOpen error\n");
		return FALSE;
	}
	/* init net thread resource */
	if (!newNetThreadResource(conf->sche.net_thread_cnt)) {
		s_BSG.errmsg = strFormat(NULL, "net thread resource create failure\n");
		return FALSE;
	}
	/* init dispatch */
	s_BSG.dispatch = newDispatch();
	if (!s_BSG.dispatch) {
		s_BSG.errmsg = strFormat(NULL, "newDispatch error\n");
		return FALSE;
	}
	/* init task thread */
	if (!def_task_thrd) {
		def_task_thrd = newTaskThreadStackCo(&conf->sche);
	}
	def_task_thrd->detached = 0;
	s_BSG.default_task_thread = def_task_thrd;
	if (!s_BSG.default_task_thread) {
		s_BSG.errmsg = strFormat(NULL, "default task thread create failure\n");
		return FALSE;
	}
	/* init ok */
	s_BSG.valid = 1;
	s_PtrBSG = &s_BSG;
	return TRUE;
}

void printBootServerNodeInfo(void) {
	fprintf(stderr, "server boot, clsnd_ident:%s, pid:%zu\n", s_BSG.conf->clsnd.ident, processId());
}

static unsigned int signal_thread_entry(void* arg) {
	threadDetach(threadSelf());
	while (s_BSG.valid) {
		int sig = signalWait();
		if (sig < 0) {
			continue;
		}
		s_BSG.sig_proc(sig);
	}
	return 0;
}

BOOL runBootServerGlobal(void) {
	BOOL retbool = FALSE;
	int task_ok = 0;
	int run = _cmpxchg32(&s_Run, 1, 0);
	if (1 == run) {
		return TRUE;
	}
	if (2 == run) {
		return FALSE;
	}
	/* run signal thread */
	if (s_BSG.sig_proc) {
		if (!threadCreate(&s_BSG.sig_tid, 0, signal_thread_entry, NULL)) {
			s_BSG.errmsg = strFormat(NULL, "signal handle thread boot failure\n");
			s_BSG.valid = 0;
			goto end;
		}
	}
	/* run task thread */
	if (!runTaskThread(s_BSG.default_task_thread)) {
		s_BSG.errmsg = strFormat(NULL, "default task thread boot failure\n");
		s_BSG.valid = 0;
		goto end;
	}
	task_ok = 1;
	/* run net thread */
	if (!runNetThreads()) {
		s_BSG.errmsg = strFormat(NULL, "net thread boot failure\n");
		s_BSG.valid = 0;
		goto end;
	}
	/* wait thread exit */
	retbool = TRUE;
end:
	if (task_ok) {
		threadJoin(s_BSG.default_task_thread->tid, NULL);
	}
	joinNetThreads();
	return retbool;
}

void stopBootServerGlobal(void) {
	if (!s_PtrBSG) {
		return;
	}
	if (_xchg32(&s_Run, 2) == 2) {
		return;
	}
	s_BSG.valid = 0;
	stopAllTaskThreads();
	wakeupNetThreads();
}

void freeBootServerGlobal(void) {
	if (!s_PtrBSG) {
		return;
	}
	waitFreeAllTaskThreads();
	s_PtrBSG = NULL;
	s_BSG.default_task_thread = NULL;
	if (s_BSG.log) {
		logDestroy(s_BSG.log);
		s_BSG.log = NULL;
	}
	if (s_BSG.conf) {
		freeBootServerConfig((BootServerConfig_t*)s_BSG.conf);
		s_BSG.conf = NULL;
	}
	freeNetThreadResource();
	if (s_BSG.dispatch) {
		freeDispatch(s_BSG.dispatch);
		s_BSG.dispatch = NULL;
	}
	if (s_BSG.errmsg) {
		free((void*)s_BSG.errmsg);
		s_BSG.errmsg = NULL;
	}
	s_Run = 0;
	s_BSG.valid = 0;
	s_BSG.argc = 0;
	s_BSG.argv = NULL;
}

#ifdef __cplusplus
}
#endif
