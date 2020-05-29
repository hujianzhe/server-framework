#include "global.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern unsigned int THREAD_CALL reactorThreadEntry(void* arg);

static void sigintHandler(int signo) {
	int i;
	g_Valid = 0;
	dataqueueWake(&g_TaskThread->dq);
	reactorWake(g_ReactorAccept);
	for (i = 0; i < g_ReactorCnt; ++i) {
		reactorWake(g_Reactors + i);
	}
}

int main(int argc, char** argv) {
	int i;
	int configinitok = 0, loginitok = 0, globalresourceinitok = 0,
		taskthreadinitok = 0, taskthreadrunok = 0, socketloopinitokcnt = 0,
		acceptthreadinitok = 0, acceptloopinitok = 0,
		listensockinitokcnt = 0;
	const char* module_path = "";
	//
	if (argc < 2) {
		return 1;
	}
	// save boot arguments
	g_MainArgc = argc;
	g_MainArgv = argv;
	// init some datastruct
	initDispatch();
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
	g_ClusterSelf = newCluster(g_Config.cluster.socktype, g_Config.cluster.ip, g_Config.cluster.port);
	if (!g_ClusterSelf)
		goto err;

	printf("cluster(%s) ip:%s, port:%u, pid:%zu\n", module_path, g_ClusterSelf->ip, g_ClusterSelf->port, processId());
	// init resource
	if (!initGlobalResource()) {
		goto err;
	}
	globalresourceinitok = 1;
	// init task thread
	g_TaskThread = newTaskThread();
	if (!g_TaskThread)
		goto err;
	taskthreadinitok = 1;
	// init reactor and start reactor thread
	if (!reactorInit(g_ReactorAccept))
		goto err;
	acceptloopinitok = 1;

	if (!threadCreate(g_ReactorAcceptThread, reactorThreadEntry, g_ReactorAccept))
		goto err;
	acceptthreadinitok = 1;

	for (; socketloopinitokcnt < g_ReactorCnt; ++socketloopinitokcnt) {
		if (!reactorInit(g_Reactors + socketloopinitokcnt)) {
			goto err;
		}
		if (!threadCreate(g_ReactorThreads + socketloopinitokcnt, reactorThreadEntry, g_Reactors + socketloopinitokcnt)) {
			goto err;
		}
	}
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
	// listen port
	for (listensockinitokcnt = 0; listensockinitokcnt < g_Config.listen_options_cnt; ++listensockinitokcnt) {
		ConfigListenOption_t* option = g_Config.listen_options + listensockinitokcnt;
		if (!strcmp(option->protocol, "http")) {
			ReactorObject_t* o = openListenerHttp(option->ip, option->port);
			if (!o)
				goto err;
			reactorCommitCmd(g_ReactorAccept, &o->regcmd);
		}
	}
	// wait thread exit
	threadJoin(g_TaskThread->tid, NULL);
	g_Valid = 0;
	threadJoin(*g_ReactorAcceptThread, NULL);
	reactorDestroy(g_ReactorAccept);
	for (i = 0; i < g_ReactorCnt; ++i) {
		threadJoin(g_ReactorThreads[i], NULL);
		reactorDestroy(g_Reactors + i);
	}
	goto end;
err:
	g_Valid = 0;
	if (acceptthreadinitok) {
		threadJoin(*g_ReactorAcceptThread, NULL);
	}
	if (acceptloopinitok) {
		reactorDestroy(g_ReactorAccept);
	}
	while (socketloopinitokcnt--) {
		threadJoin(g_ReactorThreads[socketloopinitokcnt], NULL);
		reactorDestroy(g_Reactors + socketloopinitokcnt);
	}
	if (taskthreadrunok) {
		dataqueueWake(&g_TaskThread->dq);
		threadJoin(g_TaskThread->tid, NULL);
	}
end:
	if (taskthreadinitok) {
		freeTaskThread(g_TaskThread);
	}
	if (g_ModulePtr) {
		moduleUnload(g_ModulePtr);
	}
	if (loginitok) {
		logDestroy(&g_Log);
	}
	if (configinitok) {
		freeConfig();
	}
	if (globalresourceinitok) {
		freeGlobalResource();
	}
	freeDispatchCallback();
	freeClusterTable(g_ClusterTable);
	return 0;
}
