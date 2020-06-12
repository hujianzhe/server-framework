#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "../ServiceCommCode/service_comm_cmd.h"
#include "service_center_handler.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(TaskThread_t* thrd, int argc, char** argv) {
	int i;
	const char* path = ptr_g_Config()->extra_data_txt;
	char* file_data = fileReadAllData(path, NULL);
	if (!file_data) {
		logErr(ptr_g_Log(), "fdOpen(%s) error", path);
		return 0;
	}
	if (!loadClusterNode(file_data)) {
		free(file_data);
		return 0;
	}
	free(file_data);

	regStringDispatch("/get_cluster_list", reqClusterList_http);
	regStringDispatch("/change_cluster_list", reqChangeClusterNode_http);
	regNumberDispatch(CMD_REQ_CLUSTER_LIST, reqClusterList);
	regNumberDispatch(CMD_CLUSTER_HEARTBEAT, reqClusterHeartbeat);

	// listen port
	if (getClusterSelf()->port) {
		ReactorObject_t* o = openListenerInner(getClusterSelf()->socktype, getClusterSelf()->ip, getClusterSelf()->port);
		if (!o) {
			logErr(ptr_g_Log(), "listen failure, ip:%s, port:%u ......", getClusterSelf()->ip, getClusterSelf()->port);
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

	return 1;
}

#ifdef __cplusplus
}
#endif