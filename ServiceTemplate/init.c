#include "../BootServer/config.h"
#include "../BootServer/global.h"

int init(BootServerGlobal_t* g) {
	return 0;
}

void run(struct StackCoSche_t* sche, void* arg) {
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
	}

	logInfo(ptrBSG()->log, "init ok ......");
}
