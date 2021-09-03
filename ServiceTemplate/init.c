#include "../BootServer/config.h"
#include "../BootServer/global.h"

int init(TaskThread_t* thrd, int argc, char** argv) {
	ConfigConnectOption_t* option = NULL;
	unsigned int i;

	// listen extra port
	for (i = 0; i < ptrBSG()->conf->listen_options_cnt; ++i) {
		ConfigListenOption_t* option = ptrBSG()->conf->listen_options + i;
		Channel_t* c;
		if (!strcmp(option->protocol, "http")) {
			c = openListenerHttp(option->ip, option->port, NULL, &thrd->dq);
		}
		else {
			continue;
		}
		if (!c) {
			logErr(ptrBSG()->log, "listen failure, ip:%s, port:%u ......", option->ip, option->port);
			return 0;
		}
		reactorCommitCmd(acceptReactor(), &c->_.o->regcmd);
	}

	logInfo(ptrBSG()->log, "init ok ......");
	return 1;
}

void destroy(void) {

}

#ifdef __cplusplus
}
#endif
