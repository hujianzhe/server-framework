#include "../BootServer/config.h"
#include "../BootServer/global.h"

void run(struct StackCoSche_t* sche, StackCoAsyncParam_t* param) {
	ConfigConnectOption_t* option = NULL;
	unsigned int i;

	// listen extra port
	for (i = 0; i < ptrBSG()->conf->listen_options_cnt; ++i) {
		const ConfigListenOption_t* option = ptrBSG()->conf->listen_options + i;
		NetChannel_t* c;
		if (!strcmp(option->protocol, "http")) {
			c = openNetListenerHttp(option->ip, option->port, sche);
		}
		else {
			continue;
		}
		if (!c) {
			logErr(ptrBSG()->log, "listen failure, ip:%s, port:%u ......", option->ip, option->port);
			return;
		}
		NetChannel_reg(acceptNetReactor(), c);
		NetChannel_close_ref(c);
	}

	logInfo(ptrBSG()->log, "init ok ......");
}

int init(void) {
	StackCoSche_function(ptrBSG()->default_task_thread->sche_stack_co, run, NULL);
	return 0;
}
