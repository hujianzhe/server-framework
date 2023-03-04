#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

int init(BootServerGlobal_t* g) {
	// register dispatch
	regNumberDispatch(g->dispatch, CMD_REQ_TEST, reqTest);
	regNumberDispatch(g->dispatch, CMD_REQ_TEST_CALLBACK, reqTestCallback);
	regNumberDispatch(g->dispatch, CMD_REQ_LOGIN_TEST, reqLoginTest);
	regNumberDispatch(g->dispatch, CMD_REQ_WEBSOCKET_TEST, reqWebsocketTest);
	regNumberDispatch(g->dispatch, CMD_REQ_ParallelTest1, reqParallelTest1);
	regNumberDispatch(g->dispatch, CMD_REQ_ParallelTest2, reqParallelTest2);
	regStringDispatch(g->dispatch, "/reqHttpTest", reqHttpTest);
	regStringDispatch(g->dispatch, "/reqSoTest", reqSoTest);
	regStringDispatch(g->dispatch, "/reqHttpUploadFile", reqHttpUploadFile);

	return 0;
}

void run(struct StackCoSche_t* sche, void* arg) {
	TaskThread_t* thrd = currentTaskThread();
	int i;
	// listen extra port
	for (i = 0; i < ptrBSG()->conf->listen_options_cnt; ++i) {
		const ConfigListenOption_t* option = ptrBSG()->conf->listen_options + i;
		ChannelBase_t* c;
		if (!strcmp(option->protocol, "http")) {
			c = openListenerHttp(option->ip, option->port, sche);
		}
		else {
			continue;
		}
		if (!c) {
			logErr(ptrBSG()->log, "listen failure, ip:%s, port:%u ......", option->ip, option->port);
			return;
		}
		channelbaseReg(acceptReactor(), c);
	}
}
