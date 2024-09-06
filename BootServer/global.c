#include "global.h"

static BootServerGlobal_t s_BSG, *s_PtrBSG;

#ifdef __cplusplus
extern "C" {
#endif

BootServerGlobal_t* ptrBSG(void) { return s_PtrBSG; }
const char* getBSGErrmsg(void) { return s_BSG.errmsg ? s_BSG.errmsg : ""; }

BOOL initBootServerGlobal(const BootServerConfig_t* conf, TaskThread_t* def_task_thrd, const NetScheHook_t* nsh) {
	const ConfigListenOption_t* listen_opt;

	if (s_PtrBSG) {
		return TRUE;
	}
	s_BSG.conf = conf;
	if (!nsh) {
		nsh = getNetScheHookStackCo();
	}
	s_BSG.net_sche_hook = nsh;
	/* init log */
	s_BSG.log = logOpen(conf->log.maxfilesize, conf->log.pathname);
	if (!s_BSG.log) {
		s_BSG.errmsg = strFormat(NULL, "logInit(%s) error\n", conf->log.pathname);
		return FALSE;
	}
	/* init net thread resource */
	if (!newNetThreadResource(conf->net_thread_cnt)) {
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
		def_task_thrd = newTaskThreadStackCo(conf->rpc_fiber_stack_size);
	}
	s_BSG.default_task_thread = def_task_thrd;
	if (!s_BSG.default_task_thread) {
		s_BSG.errmsg = strFormat(NULL, "default task thread create failure\n");
		return FALSE;
	}
	/* listen self cluster node port, if needed */
	listen_opt = &conf->clsnd.listen_option;
	if (!strcmp(listen_opt->protocol, "default")) {
		NetChannel_t* c = openNetListenerInner(listen_opt->socktype, listen_opt->ip, listen_opt->port, s_BSG.default_task_thread->sche);
		if (!c) {
			s_BSG.errmsg = strFormat(NULL, "listen self cluster node err, ip:%s, port:%u\n", listen_opt->ip, listen_opt->port);
			return FALSE;
		}
		NetChannel_reg(acceptNetReactor(), c);
		NetChannel_close_ref(c);
	}
	/* init ok */
	s_BSG.valid = 1;
	s_PtrBSG = &s_BSG;
	return TRUE;
}

void printBootServerNodeInfo(void) {
	const ConfigListenOption_t* listen_opt = &s_BSG.conf->clsnd.listen_option;

	fprintf(stderr, "server boot, clsnd_ident:%s, socktype:%s, ip:%s, port:%u, pid:%zu\n",
		s_BSG.conf->clsnd.ident, if_socktype2string(listen_opt->socktype),
		listen_opt->ip, listen_opt->port, processId());
}

static unsigned int signal_thread_entry(void* arg) {
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
	int sig_ok = 0;
	int task_ok = 0;
	/* run signal thread */
	if (s_BSG.sig_proc) {
		if (!threadCreate(&s_BSG.sig_tid, signal_thread_entry, NULL)) {
			s_BSG.errmsg = strFormat(NULL, "signal handle thread boot failure\n");
			goto end;
		}
		sig_ok = 1;
	}
	/* run task thread */
	if (!runTaskThread(s_BSG.default_task_thread)) {
		s_BSG.errmsg = strFormat(NULL, "default task thread boot failure\n");
		goto end;
	}
	task_ok = 1;
	/* run net thread */
	if (!runNetThreads()) {
		s_BSG.errmsg = strFormat(NULL, "net thread boot failure\n");
		goto end;
	}
	/* wait thread exit */
	retbool = TRUE;
end:
	if (task_ok) {
		threadJoin(s_BSG.default_task_thread->tid, NULL);
	}
	s_BSG.valid = 0;
	joinNetThreads();
	if (sig_ok) {
		threadJoin(s_BSG.sig_tid, NULL);
	}
	return retbool;
}

void stopBootServerGlobal(void) {
	if (!s_PtrBSG) {
		return;
	}
	s_BSG.valid = 0;
	if (s_BSG.default_task_thread) {
		s_BSG.default_task_thread->hook->exit(s_BSG.default_task_thread);
	}
	wakeupNetThreads();
}

void freeBootServerGlobal(void) {
	if (!s_PtrBSG) {
		return;
	}
	s_PtrBSG = NULL;
	if (s_BSG.default_task_thread) {
		freeTaskThread(s_BSG.default_task_thread);
		s_BSG.default_task_thread = NULL;
	}
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
	s_BSG.valid = 0;
	s_BSG.argc = 0;
	s_BSG.argv = NULL;
}

#ifdef __cplusplus
}
#endif
