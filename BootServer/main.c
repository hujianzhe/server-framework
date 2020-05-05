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
	void* module_ptr = NULL;
	void(*module_destroy_fn_ptr)(void) = NULL;
	//
	if (!initConfig(argc > 1 ? argv[1] : "config.txt")) {
		return 1;
	}
	printf("cluster_name:%s, pid:%zu\n", g_Config.cluster_name, processId());

	if (!initGlobalResource()) {
		return 1;
	}

	initDispatch();

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
	if (g_Config.module_path) {
		int(*init_fn_ptr)(int, char**);
		module_ptr = moduleLoad(g_Config.module_path);
		if (!module_ptr) {
			printf("moduleLoad(%s) failure\n", g_Config.module_path);
			goto err;
		}
		module_destroy_fn_ptr = (void(*)(void))moduleSymbolAddress(module_ptr, "destroy");

		init_fn_ptr = (int(*)(int, char**))moduleSymbolAddress(module_ptr, "init");
		if (!init_fn_ptr) {
			printf("moduleSymbolAddress(%s, \"init\") failure\n", g_Config.module_path);
			goto err;
		}
		if (!init_fn_ptr(argc, argv)) {
			printf("(%s).init(argc, argv) return failure\n", g_Config.module_path);
			goto err;
		}
	}
	//
	threadJoin(g_TaskThread, NULL);
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
	if (module_ptr) {
		if (module_destroy_fn_ptr) {
			module_destroy_fn_ptr();
			module_destroy_fn_ptr = NULL;
		}
		moduleUnload(module_ptr);
	}
	freeConfig();
	freeDispatchCallback();
	freeGlobalResource();
	return 0;
}
