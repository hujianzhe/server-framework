#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "../InnerProcHandle/inner_proc_cmd.h"
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

	if (getClusterSelf()->port) {
		ReactorObject_t* o = openListenerInner(getClusterSelf()->socktype, getClusterSelf()->ip, getClusterSelf()->port);
		if (!o)
			return 0;
		reactorCommitCmd(ptr_g_ReactorAccept(), &o->regcmd);
	}

	return 1;
}

#ifdef __cplusplus
}
#endif