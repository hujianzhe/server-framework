#include "../BootServer/config.h"
#include "../BootServer/global.h"

static void net_dispatch(TaskThread_t* thrd, DispatchNetMsg_t* net_msg) {
	net_msg->callback(thrd, net_msg);
}

void run(struct StackCoSche_t* sche, StackCoAsyncParam_t* param) {
	ConfigConnectOption_t* option = NULL;
	unsigned int i;

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
		channelbaseCloseRef(c);
	}

	logInfo(ptrBSG()->log, "init ok ......");
}

int init(BootServerGlobal_t* g) {
	g->default_task_thread->net_dispatch = net_dispatch;
	StackCoSche_function(g->default_task_thread->sche, run, NULL);
	return 0;
}
