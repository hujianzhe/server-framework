#include "global.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_STATIC_MODULE
#ifdef __cplusplus
extern "C" {
#endif
	int init(TaskThread_t*, int, char**);
#ifdef __cplusplus
}
#endif
#endif


static void sigintHandler(int signo) {
	g_Valid = 0;
	dataqueueWake(&g_TaskThread->dq);
	wakeupNetThreads();
}

int main(int argc, char** argv) {
	int configinitok = 0, loginitok = 0, netthreadresourceinitok = 0,
		taskthreadinitok = 0, taskthreadrunok = 0;
	struct ClusterTable_t* clstbl = NULL;
	//
	if (argc < 2) {
		fputs("need a config file to boot ...", stderr);
		return 1;
	}
	// save boot arguments
	g_MainArgc = argc;
	g_MainArgv = argv;
	// load config
	if (!initConfig(argv[1])) {
		fprintf(stderr, "initConfig(%s) error\n", argv[1]);
		goto err;
	}
	configinitok = 1;
	// init log
	if (!logInit(&g_Log, "", g_Config.log.pathname)) {
		fprintf(stderr, "logInit(%s) error\n", g_Config.log.pathname);
		goto err;
	}
	g_Log.m_maxfilesize = g_Config.log.maxfilesize;
	loginitok = 1;
#ifdef USE_STATIC_MODULE
	g_ModuleInitFunc = init;
#else
	// load module
	if (g_Config.module_path && g_Config.module_path[0]) {
		g_ModulePtr = moduleLoad(g_Config.module_path);
		if (!g_ModulePtr) {
			fprintf(stderr, "moduleLoad(%s) failure\n", g_Config.module_path);
			logErr(&g_Log, "moduleLoad(%s) failure", g_Config.module_path);
			goto err;
		}
		g_ModuleInitFunc = (int(*)(TaskThread_t*, int, char**))moduleSymbolAddress(g_ModulePtr, "init");
		if (!g_ModuleInitFunc) {
			fprintf(stderr, "moduleSymbolAddress(%s, \"init\") failure\n", g_Config.module_path);
			logErr(&g_Log, "moduleSymbolAddress(%s, \"init\") failure", g_Config.module_path);
			goto err;
		}
	}
#endif
	// input boot cluster node info
	logInfo(&g_Log, "module_path(%s) name:%s, socktype:%s, ip:%s, port:%u, pid:%zu",
		g_Config.module_path ? g_Config.module_path : "", g_Config.clsnd.name,
		if_socktype2string(g_Config.clsnd.socktype), g_Config.clsnd.ip, g_Config.clsnd.port, processId());

	printf("module_path(%s) name:%s, socktype:%s, ip:%s, port:%u, pid:%zu\n",
		g_Config.module_path ? g_Config.module_path : "", g_Config.clsnd.name,
		if_socktype2string(g_Config.clsnd.socktype), g_Config.clsnd.ip, g_Config.clsnd.port, processId());
	// init cluster data
	if (g_Config.cluster_table_path && g_Config.cluster_table_path[0]) {
		ClusterNode_t* clsnd;
		const char* load_cluster_table_errmsg;
		char* cluster_table_filedata = fileReadAllData(g_Config.cluster_table_path, NULL);
		if (!cluster_table_filedata) {
			fprintf(stderr, "fileReadAllData(%s) failure\n", g_Config.cluster_table_path);
			logErr(&g_Log, "fileReadAllData(%s) failure", g_Config.cluster_table_path);
			goto err;
		}
		clstbl = loadClusterTableFromJsonData(cluster_table_filedata, &load_cluster_table_errmsg);
		free(cluster_table_filedata);
		if (!clstbl) {
			fprintf(stderr, "loadClusterTableFromJsonData failure: %s\n", load_cluster_table_errmsg);
			logErr(&g_Log, "loadClusterTableFromJsonData failure: %s", load_cluster_table_errmsg);
			goto err;
		}
		clsnd = getClusterNodeFromGroup(
			getClusterNodeGroup(clstbl, g_Config.clsnd.name),
			g_Config.clsnd.socktype,
			g_Config.clsnd.ip,
			g_Config.clsnd.port
		);
		if (!clsnd || clsnd->id != g_Config.clsnd.id) {
			fprintf(stderr, "self cluster node isn't find, name:%s, id:%d, socktype:%s, ip:%s, port:%u\n",
				g_Config.clsnd.name,
				g_Config.clsnd.id,
				if_socktype2string(g_Config.clsnd.socktype),
				g_Config.clsnd.ip,
				g_Config.clsnd.port
			);
			logErr(&g_Log, "self cluster node isn't find, name:%s, id:%d, socktype:%s, ip:%s, port:%u",
				g_Config.clsnd.name,
				g_Config.clsnd.id,
				if_socktype2string(g_Config.clsnd.socktype),
				g_Config.clsnd.ip,
				g_Config.clsnd.port
			);
			return 0;
		}
	}
	else {
		ClusterNode_t* clsnd;
		clsnd = newClusterNode(g_Config.clsnd.id, g_Config.clsnd.socktype, g_Config.clsnd.ip, g_Config.clsnd.port);
		if (!clsnd) {
			fprintf(stderr, "new self cluster node failure, name:%s, socktype:%s, ip:%s, port:%u",
				g_Config.clsnd.name,
				if_socktype2string(g_Config.clsnd.socktype),
				g_Config.clsnd.ip,
				g_Config.clsnd.port
			);
			logErr(&g_Log, "new self cluster node failure, name:%s, socktype:%s, ip:%s, port:%u",
				g_Config.clsnd.name,
				if_socktype2string(g_Config.clsnd.socktype),
				g_Config.clsnd.ip,
				g_Config.clsnd.port
			);
			goto err;
		}
		clstbl = newClusterTable();
		if (!clstbl)
			goto err;
		if (!regClusterNode(clstbl, g_Config.clsnd.name, clsnd)) {
			fprintf(stderr, "reg self cluster node failure, name:%s, socktype:%s, ip:%s, port:%u",
				g_Config.clsnd.name,
				if_socktype2string(g_Config.clsnd.socktype),
				g_Config.clsnd.ip,
				g_Config.clsnd.port
			);
			logErr(&g_Log, "reg self cluster node failure, name:%s, socktype:%s, ip:%s, port:%u",
				g_Config.clsnd.name,
				if_socktype2string(g_Config.clsnd.socktype),
				g_Config.clsnd.ip,
				g_Config.clsnd.port
			);
			goto err;
		}
	}
	// init net thread resource
	if (!newNetThreadResource(g_Config.net_thread_cnt)) {
		goto err;
	}
	netthreadresourceinitok = 1;
	// listen self cluster node port
	if (g_Config.clsnd.port) {
		Channel_t* c = openListenerInner(g_Config.clsnd.socktype, g_Config.clsnd.ip, g_Config.clsnd.port);
		if (!c) {
			fprintf(stderr, "listen self cluster node err, ip:%s, port:%u\n", g_Config.clsnd.ip, g_Config.clsnd.port);
			logErr(&g_Log, "listen self cluster node err, ip:%s, port:%u", g_Config.clsnd.ip, g_Config.clsnd.port);
			goto err;
		}
		reactorCommitCmd(ptr_g_ReactorAccept(), &c->_.o->regcmd);
	}
	// init task thread
	g_TaskThread = newTaskThread();
	if (!g_TaskThread)
		goto err;
	taskthreadinitok = 1;
	g_TaskThread->clstbl = clstbl;
	// run reactor thread
	if (!runNetThreads())
		goto err;
	// reg SIGINT signal
	if (signalRegHandler(SIGINT, sigintHandler) == SIG_ERR)
		goto err;
	// run task thread
	if (!runTaskThread(g_TaskThread))
		goto err;
	taskthreadrunok = 1;
	// post module init_func message
	if (g_ModuleInitFunc) {
		UserMsg_t* msg = newUserMsg(0);
		if (!msg)
			goto err;
		dataqueuePush(&g_TaskThread->dq, &msg->internal._);
	}
	// wait thread exit
	threadJoin(g_TaskThread->tid, NULL);
	g_Valid = 0;
	joinNetThreads();
	goto end;
err:
	g_Valid = 0;
	joinNetThreads();
	if (taskthreadrunok) {
		dataqueueWake(&g_TaskThread->dq);
		threadJoin(g_TaskThread->tid, NULL);
	}
end:
	if (taskthreadinitok) {
		freeTaskThread(g_TaskThread);
	}
	if (g_ModulePtr) {
		(void)moduleUnload(g_ModulePtr);
	}
	if (loginitok) {
		logDestroy(&g_Log);
	}
	if (configinitok) {
		freeConfig();
	}
	if (netthreadresourceinitok) {
		freeNetThreadResource();
	}
	if (clstbl)
		freeClusterTable(clstbl);
	return 0;
}
