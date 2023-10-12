#include "global.h"

static Config_t s_Config;
static BootServerGlobal_t s_BSG, *s_PtrBSG;

#ifdef __cplusplus
extern "C" {
#endif

BootServerGlobal_t* ptrBSG(void) { return s_PtrBSG; }
const char* getBSGErrmsg(void) { return s_BSG.errmsg ? s_BSG.errmsg : ""; }
int checkStopBSG(void) { return s_PtrBSG ? !(s_PtrBSG->valid) : 1; }

BOOL initBootServerGlobal(const char* conf_path, int argc, char** argv, int(*fn_init)(BootServerGlobal_t*)) {
	ConfigListenOption_t* listen_opt;

	if (s_PtrBSG) {
		return TRUE;
	}
	s_BSG.argc = argc;
	s_BSG.argv = argv;
	// load config
	if (!initConfig(conf_path, &s_Config)) {
		s_BSG.errmsg = strFormat(NULL, "initConfig(%s) error\n", conf_path);
		return FALSE;
	}
	s_BSG.conf = &s_Config;
	// init log
	s_BSG.log = logOpen(s_Config.log.maxfilesize, s_Config.log.pathname);
	if (!s_BSG.log) {
		s_BSG.errmsg = strFormat(NULL, "logInit(%s) error\n", s_Config.log.pathname);
		return FALSE;
	}
	// init net thread resource
	if (!newNetThreadResource(s_Config.net_thread_cnt)) {
		s_BSG.errmsg = strFormat(NULL, "net thread resource create failure\n");
		return FALSE;
	}
	// init dispatch
	s_BSG.dispatch = newDispatch();
	if (!s_BSG.dispatch) {
		s_BSG.errmsg = strFormat(NULL, "newDispatch error\n");
		return FALSE;
	}
	// init task thread
	s_BSG.default_task_thread = newTaskThread(s_Config.rpc_fiber_stack_size);
	if (!s_BSG.default_task_thread) {
		s_BSG.errmsg = strFormat(NULL, "default task thread create failure\n");
		return FALSE;
	}
	// init user global
	if (fn_init) {
		s_PtrBSG = &s_BSG;
		int ret = fn_init(&s_BSG);
		if (ret) {
			s_PtrBSG = NULL;
			s_BSG.errmsg = strFormat(NULL, "initBootServerGlobal call fn_init err, ret=%d\n", ret);
			return FALSE;
		}
	}
	// listen self cluster node port, if needed
	listen_opt = &s_Config.clsnd.listen_option;
	if (!strcmp(listen_opt->protocol, "default")) {
		ChannelBase_t* c = openListenerInner(listen_opt->socktype, listen_opt->ip, listen_opt->port, s_BSG.default_task_thread->sche);
		if (!c) {
			s_BSG.errmsg = strFormat(NULL, "listen self cluster node err, ip:%s, port:%u\n", listen_opt->ip, listen_opt->port);
			return FALSE;
		}
		channelbaseReg(acceptReactor(), c);
		channelbaseCloseRef(c);
	}
	// init ok
	s_BSG.valid = 1;
	s_PtrBSG = &s_BSG;
	return TRUE;
}

void printBootServerNodeInfo(void) {
	ConfigListenOption_t* listen_opt = &s_Config.clsnd.listen_option;

	fprintf(stderr, "server boot, clsnd_ident:%s, socktype:%s, ip:%s, port:%u, pid:%zu\n",
		s_Config.clsnd.ident, if_socktype2string(listen_opt->socktype),
		listen_opt->ip, listen_opt->port, processId());
}

BOOL runBootServerGlobal(void) {
	// run task thread
	if (!runTaskThread(s_BSG.default_task_thread)) {
		s_BSG.errmsg = strFormat(NULL, "default task thread boot failure\n");
		return FALSE;
	}
	// run net thread
	if (!runNetThreads()) {
		s_BSG.errmsg = strFormat(NULL, "net thread run failure\n");
		return FALSE;
	}
	// wait thread exit
	threadJoin(s_BSG.default_task_thread->tid, NULL);
	s_BSG.valid = 0;
	joinNetThreads();
	return TRUE;
}

void stopBootServerGlobal(void) {
	if (!s_PtrBSG) {
		return;
	}
	s_BSG.valid = 0;
	if (s_BSG.default_task_thread) {
		StackCoSche_exit(s_BSG.default_task_thread->sche);
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
		resetConfig((Config_t*)s_BSG.conf);
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
