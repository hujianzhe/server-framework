#include "../BootServer/config.h"
#include "../BootServer/global.h"

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(TaskThread_t* thrd, int argc, char** argv) {
	ConfigConnectOption_t* option = NULL;
	unsigned int i;

	// listen extra port
	for (i = 0; i < ptr_g_Config()->listen_options_cnt; ++i) {
		ConfigListenOption_t* option = ptr_g_Config()->listen_options + i;
		Channel_t* c;
		if (!strcmp(option->protocol, "http")) {
			c = openListenerHttp(option->ip, option->port, NULL, &thrd->dq);
		}
		else {
			continue;
		}
		if (!c) {
			logErr(ptr_g_Log(), "listen failure, ip:%s, port:%u ......", option->ip, option->port);
			return 0;
		}
		reactorCommitCmd(acceptReactor(), &c->_.o->regcmd);
	}

	logInfo(ptr_g_Log(), "init ok ......");
	return 1;
}

__declspec_dllexport void destroy(void) {

}

#ifdef __cplusplus
}
#endif
