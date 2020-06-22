#include "global.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sigintHandler(int signo) {
	g_Valid = 0;
	dataqueueWake(&g_TaskThread->dq);
	wakeupNetThreads();
}

int main(int argc, char** argv) {
	int configinitok = 0, loginitok = 0, netthreadresourceinitok = 0,
		taskthreadinitok = 0, taskthreadrunok = 0;
	const char* module_path = "";
	//
	if (argc < 2) {
		return 1;
	}
	// save boot arguments
	g_MainArgc = argc;
	g_MainArgv = argv;
	// init some datastruct
	g_ClusterTable = newClusterTable();
	if (!g_ClusterTable)
		goto err;
	// load config
	if (!initConfig(argv[1])) {
		fprintf(stderr, "initConfig(%s) error", argv[1]);
		goto err;
	}
	configinitok = 1;
	// init log
	if (!logInit(&g_Log, "", g_Config.log.pathname)) {
		fprintf(stderr, "logInit(%s) error", g_Config.log.pathname);
		goto err;
	}
	g_Log.m_maxfilesize = g_Config.log.maxfilesize;
	loginitok = 1;
	// load module
	if (argc > 2) {
		module_path = argv[2];
		if (module_path[0]) {
			free((char*)g_Config.module_path);
			g_Config.module_path = strdup(module_path);
			if (!g_Config.module_path)
				goto err;
		}
	}
	if ('\0' == module_path[0] && g_Config.module_path) {
		module_path = g_Config.module_path;
	}
	if (module_path[0]) {
		g_ModulePtr = moduleLoad(module_path);
		if (!g_ModulePtr) {
			fprintf(stderr, "moduleLoad(%s) failure\n", module_path);
			goto err;
		}
		g_ModuleInitFunc = (int(*)(TaskThread_t*, int, char**))moduleSymbolAddress(g_ModulePtr, "init");
		if (!g_ModuleInitFunc) {
			fprintf(stderr, "moduleSymbolAddress(%s, \"init\") failure\n", module_path);
			goto err;
		}
	}
	// init cluster self
	g_SelfClusterNode = newClusterNode(g_Config.cluster.socktype, g_Config.cluster.ip, g_Config.cluster.port);
	if (!g_SelfClusterNode)
		goto err;
	g_SelfClusterNode->name = g_Config.cluster.name;

	printf("cluster(%s) name:%s, ip:%s, port:%u, pid:%zu\n",
		module_path, g_Config.cluster.name, g_SelfClusterNode->ip, g_SelfClusterNode->port, processId());
	// init net thread resource
	if (!newNetThreadResource()) {
		goto err;
	}
	netthreadresourceinitok = 1;
	// init task thread
	g_TaskThread = newTaskThread();
	if (!g_TaskThread)
		goto err;
	taskthreadinitok = 1;
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
	if (g_ClusterTable)
		freeClusterTable(g_ClusterTable);
	return 0;
}
