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
	int dqinitok = 0, timerinitok = 0, timerrpcinitok = 0,
		taskthreadinitok = 0, socketloopinitokcnt = 0,
		acceptthreadinitok = 0, acceptloopinitok = 0,
		listensockinitokcnt = 0;
	const char* conf_path = argc > 1 ? argv[1] : "config.txt";
	//
	g_MainArgc = argc;
	g_MainArgv = argv;
	if (!initConfig(conf_path)) {
		printf("initConfig(%s) error\n", conf_path);
		return 1;
	}
	if (g_Config.module_path) {
		g_ModulePtr = moduleLoad(g_Config.module_path);
		if (!g_ModulePtr) {
			printf("moduleLoad(%s) failure\n", g_Config.module_path);
			freeConfig();
			return 1;
		}
	}
	printf("cluster_group_name:%s, pid:%zu\n", g_Config.cluster.group_name, processId());

	initClusterTable();
	initDispatch();

	if (!initGlobalResource()) {
		goto err;
	}

	g_ClusterSelf = newCluster();
	if (!g_ClusterSelf) {
		goto err;
	}
	strcpy(g_ClusterSelf->ip, g_Config.cluster.ip);
	g_ClusterSelf->port = g_Config.cluster.port;
	if (!regCluster(g_Config.cluster.group_name, g_ClusterSelf)) {
		goto err;
	}

	if (!dataqueueInit(&g_DataQueue))
		goto err;
	dqinitok = 1;

	if (!rbtimerInit(&g_Timer, TRUE))
		goto err;
	timerinitok = 1;
	if (!rbtimerInit(&g_TimerRpcTimeout, TRUE))
		goto err;
	timerrpcinitok = 1;

	if (!threadCreate(&g_TaskThread, taskThreadEntry, NULL))
		goto err;
	taskthreadinitok = 1;

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

	if (signalRegHandler(SIGINT, sigintHandler) == SIG_ERR)
		goto err;

	for (listensockinitokcnt = 0; listensockinitokcnt < g_Config.listen_options_cnt; ++listensockinitokcnt) {
		ConfigListenOption_t* option = g_Config.listen_options + listensockinitokcnt;
		if (!strcmp(option->protocol, "inner")) {
			int domain = ipstrFamily(option->ip);
			ReactorObject_t* o = openListener(domain, option->socktype, option->ip, option->port);
			if (!o)
				goto err;
			reactorCommitCmd(g_ReactorAccept, &o->regcmd);
		}
		else if (!strcmp(option->protocol, "http")) {
			int domain = ipstrFamily(option->ip);
			ReactorObject_t* o = openListenerHttp(domain, option->ip, option->port);
			if (!o)
				goto err;
			reactorCommitCmd(g_ReactorAccept, &o->regcmd);
		}
	}
	//
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
	freeConfig();
	freeDispatchCallback();
	freeClusterTable();
	freeGlobalResource();
	return 0;
}
