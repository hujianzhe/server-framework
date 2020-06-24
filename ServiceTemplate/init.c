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
	ClusterNode_t* clsnd;
	unsigned int i;

	// listen port
	if (selfClusterNode()->port) {
		ReactorObject_t* o = openListenerInner(selfClusterNode()->socktype, selfClusterNode()->ip, selfClusterNode()->port);
		if (!o) {
			logErr(ptr_g_Log(), "listen failure, ip:%s, port:%u ......", selfClusterNode()->ip, selfClusterNode()->port);
			return 0;
		}
		reactorCommitCmd(ptr_g_ReactorAccept(), &o->regcmd);
	}

	for (i = 0; i < ptr_g_Config()->listen_options_cnt; ++i) {
		ConfigListenOption_t* option = ptr_g_Config()->listen_options + i;
		ReactorObject_t* o;
		if (!strcmp(option->protocol, "http")) {
			o = openListenerHttp(option->ip, option->port, NULL);
		}
		else {
			continue;
		}
		if (!o) {
			logErr(ptr_g_Log(), "listen failure, ip:%s, port:%u ......", option->ip, option->port);
			return 0;
		}
		reactorCommitCmd(ptr_g_ReactorAccept(), &o->regcmd);
	}

	logInfo(ptr_g_Log(), "init ok ......");
	return 1;
}

__declspec_dllexport void destroy(void) {

}

#ifdef __cplusplus
}
#endif