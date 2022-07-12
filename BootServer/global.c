#include "global.h"

static Log_t s_Log;
static Config_t s_Config;
static BootServerGlobal_t s_BSG, *s_PtrBSG;

#ifdef __cplusplus
extern "C" {
#endif

BootServerGlobal_t* ptrBSG(void) { return s_PtrBSG; }
const char* getBSGErrmsg(void) { return s_BSG.errmsg ? s_BSG.errmsg : ""; }
int checkStopBSG(void) { return s_PtrBSG ? !(s_PtrBSG->valid) : 1; }

BOOL initBootServerGlobal(const char* conf_path) {
	if (s_PtrBSG) {
		return TRUE;
	}
	// load config
	if (!initConfig(conf_path, &s_Config)) {
		s_BSG.errmsg = strFormat(NULL, "initConfig(%s) error\n", conf_path);
		return FALSE;
	}
	s_BSG.conf = &s_Config;
	// init log
	if (!logInit(&s_Log, "", s_Config.log.pathname)) {
		s_BSG.errmsg = strFormat(NULL, "logInit(%s) error\n", s_Config.log.pathname);
		return FALSE;
	}
	s_Log.m_maxfilesize = s_Config.log.maxfilesize;
	s_BSG.log = &s_Log;
	// init net thread resource
	if (!newNetThreadResource(s_Config.net_thread_cnt)) {
		s_BSG.errmsg = strFormat(NULL, "net thread resource create failure\n");
		return FALSE;
	}
	// init task thread
	s_BSG.default_task_thread = newTaskThread();
	if (!s_BSG.default_task_thread) {
		s_BSG.errmsg = strFormat(NULL, "default task thread create failure\n");
		return FALSE;
	}
	// init cluster data
	s_BSG.default_task_thread->clstbl = newClusterTable();
	if (!s_BSG.default_task_thread->clstbl) {
		s_BSG.errmsg = strFormat(NULL, "newClusterTable failure\n");
		return FALSE;
	}
	if (s_Config.cluster_table_path && s_Config.cluster_table_path[0]) {
		ClusterNode_t* clsnd;
		const char* load_cluster_table_errmsg;
		char* cluster_table_filedata = fileReadAllData(s_Config.cluster_table_path, NULL);
		if (!cluster_table_filedata) {
			s_BSG.errmsg = strFormat(NULL, "fileReadAllData(%s) failure\n", s_Config.cluster_table_path);
			return FALSE;
		}
		if (!loadClusterTableFromJsonData(s_BSG.default_task_thread->clstbl, cluster_table_filedata, &load_cluster_table_errmsg)) {
			s_BSG.errmsg = strFormat(NULL, "loadClusterTableFromJsonData failure: %s\n", load_cluster_table_errmsg);
			free(cluster_table_filedata);
			return FALSE;
		}
		free(cluster_table_filedata);
		clsnd = getClusterNodeById(s_BSG.default_task_thread->clstbl, s_Config.clsnd.ident);
		if (!clsnd || strcmp(clsnd->ident, s_Config.clsnd.ident)) {
			s_BSG.errmsg = strFormat(NULL, "self cluster node(ident:%s) isn't in cluster table\n", s_Config.clsnd.ident);
			return FALSE;
		}
		if (clsnd->socktype != s_Config.clsnd.socktype ||
			clsnd->port != s_Config.clsnd.port ||
			strcmp(clsnd->ip, s_Config.clsnd.ip))
		{
			s_BSG.errmsg = strFormat(NULL, "self cluster node isn't find, ident:%s, socktype:%s, ip:%s, port:%u\n",
				s_Config.clsnd.ident,
				if_socktype2string(s_Config.clsnd.socktype),
				s_Config.clsnd.ip,
				s_Config.clsnd.port
			);
			return FALSE;
		}
	}
	// init ok
	s_BSG.valid = 1;
	s_PtrBSG = &s_BSG;
	return TRUE;
}

void printBootServerNodeInfo(void) {
	logInfo(&s_Log, "server boot, clsnd_ident:%s, socktype:%s, ip:%s, port:%u, pid:%zu",
		s_Config.clsnd.ident, if_socktype2string(s_Config.clsnd.socktype),
		s_Config.clsnd.ip, s_Config.clsnd.port, processId());

	fprintf(stderr, "server boot, clsnd_ident:%s, socktype:%s, ip:%s, port:%u, pid:%zu\n",
		s_Config.clsnd.ident, if_socktype2string(s_Config.clsnd.socktype),
		s_Config.clsnd.ip, s_Config.clsnd.port, processId());
}

BOOL runBootServerGlobal(int argc, char** argv, int(*fn_init)(TaskThread_t*, int, char**), void(*fn_destroy)(TaskThread_t*)) {
	// listen self cluster node port
	if (s_Config.clsnd.port) {
		Channel_t* c = openListenerInner(s_Config.clsnd.socktype, s_Config.clsnd.ip, s_Config.clsnd.port, &s_BSG.default_task_thread->dq);
		if (!c) {
			s_BSG.errmsg = strFormat(NULL, "listen self cluster node err, ip:%s, port:%u\n", s_Config.clsnd.ip, s_Config.clsnd.port);
			return FALSE;
		}
		reactorCommitCmd(acceptReactor(), &c->_.o->regcmd);
	}
	// run net thread
	if (!runNetThreads()) {
		s_BSG.errmsg = strFormat(NULL, "net thread run failure\n");
		return FALSE;
	}
	// run task thread
	s_BSG.argc = argc;
	s_BSG.argv = argv;
	s_BSG.default_task_thread->init_argc = argc;
	s_BSG.default_task_thread->init_argv = argv;
	s_BSG.default_task_thread->fn_init = fn_init;
	s_BSG.default_task_thread->fn_destroy = fn_destroy;
	if (!runTaskThread(s_BSG.default_task_thread)) {
		s_BSG.errmsg = strFormat(NULL, "default task thread boot failure\n");
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
		dataqueueWake(&s_BSG.default_task_thread->dq);
	}
	wakeupNetThreads();
}

void freeBootServerGlobal(void) {
	if (!s_PtrBSG) {
		return;
	}
	s_PtrBSG = NULL;
	if (s_BSG.default_task_thread) {
		freeClusterTable(s_BSG.default_task_thread->clstbl);
		freeTaskThread(s_BSG.default_task_thread);
		s_BSG.default_task_thread = NULL;
	}
	if (s_BSG.log) {
		logDestroy(s_BSG.log);
		s_BSG.log = NULL;
	}
	if (s_BSG.conf) {
		resetConfig(s_BSG.conf);
		s_BSG.conf = NULL;
	}
	freeNetThreadResource();
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
