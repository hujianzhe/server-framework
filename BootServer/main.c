#include "global.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int THREAD_CALL reactorThreadEntry(void* arg);
unsigned int THREAD_CALL taskThreadEntry(void* arg);

static void sigintHandler(int signo) {
	int i;
	g_Valid = 0;
	dataqueueWake(&g_DataQueue);
	reactorWake(g_ReactorAccept);
	for (i = 0; i < g_ReactorCnt; ++i) {
		reactorWake(g_Reactors + i);
	}
}

int main(int argc, char** argv) {
	int i;
	int configinitok = 0, loginitok = 0, globalresourceinitok = 0,
		dqinitok = 0, timerinitok = 0, timerrpcinitok = 0,
		taskthreadinitok = 0, socketloopinitokcnt = 0,
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
	initClusterTable();
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
		g_ModuleInitFunc = (int(*)(int, char**))moduleSymbolAddress(g_ModulePtr, "init");
		if (!g_ModuleInitFunc) {
			fprintf(stderr, "moduleSymbolAddress(%s, \"init\") failure\n", module_path);
			goto err;
		}
	}
	// init cluster self
	g_ClusterSelf = newCluster();
	if (!g_ClusterSelf)
		goto err;
	g_ClusterSelf->socktype = g_Config.cluster.socktype;
	strcpy(g_ClusterSelf->ip, g_Config.cluster.ip);
	g_ClusterSelf->port = g_Config.cluster.port;

	printf("cluster(%s) ip:%s, port:%u, pid:%zu\n", module_path, g_ClusterSelf->ip, g_ClusterSelf->port, processId());
	// init resource
	if (!initGlobalResource()) {
		goto err;
	}
	globalresourceinitok = 1;
	// init queue
	if (!dataqueueInit(&g_DataQueue))
		goto err;
	dqinitok = 1;
	// init timer
	if (!rbtimerInit(&g_Timer, TRUE))
		goto err;
	timerinitok = 1;
	if (!rbtimerInit(&g_TimerRpcTimeout, TRUE))
		goto err;
	timerrpcinitok = 1;
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
	// start task thread
	if (!threadCreate(&g_TaskThread, taskThreadEntry, NULL))
		goto err;
	taskthreadinitok = 1;
	// post module init_func message
	if (g_ModuleInitFunc) {
		UserMsg_t* msg = newUserMsg(0);
		if (!msg)
			goto err;
		dataqueuePush(&g_DataQueue, &msg->internal._);
	}
	// listen port
	if (g_ClusterSelf->port) {
		int domain = ipstrFamily(g_ClusterSelf->ip);
		ReactorObject_t* o = openListener(domain, g_ClusterSelf->socktype, g_ClusterSelf->ip, g_ClusterSelf->port);
		if (!o)
			goto err;
		reactorCommitCmd(g_ReactorAccept, &o->regcmd);
	}
	for (listensockinitokcnt = 0; listensockinitokcnt < g_Config.listen_options_cnt; ++listensockinitokcnt) {
		ConfigListenOption_t* option = g_Config.listen_options + listensockinitokcnt;
		if (!strcmp(option->protocol, "http")) {
			int domain = ipstrFamily(option->ip);
			ReactorObject_t* o = openListenerHttp(domain, option->ip, option->port);
			if (!o)
				goto err;
			reactorCommitCmd(g_ReactorAccept, &o->regcmd);
		}
	}
	// wait thread exit
	threadJoin(g_TaskThread, NULL);
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
	if (taskthreadinitok) {
		dataqueueWake(&g_DataQueue);
		threadJoin(g_TaskThread, NULL);
	}
end:
	if (dqinitok) {
		dataqueueDestroy(&g_DataQueue);
	}
	if (timerinitok) {
		rbtimerDestroy(&g_Timer);
	}
	if (timerrpcinitok) {
		rbtimerDestroy(&g_TimerRpcTimeout);
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
	freeClusterTable();
	return 0;
}
