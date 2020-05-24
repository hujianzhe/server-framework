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

__declspec_dllexport int init(int argc, char** argv) {
	if (!loadClusterNode(ptr_g_Config()->extra_data_txt))
		return 0;

	regStringDispatch("/get_cluster_list", reqClusterList_http);
	regNumberDispatch(CMD_REQ_CLUSTER_LIST, reqClusterList);

	if (ptr_g_ClusterSelf()->port) {
		ReactorObject_t* o = openListener(ptr_g_ClusterSelf()->socktype, ptr_g_ClusterSelf()->ip, ptr_g_ClusterSelf()->port);
		if (!o)
			return 0;
		reactorCommitCmd(ptr_g_ReactorAccept(), &o->regcmd);
	}

	return 1;
}

#ifdef __cplusplus
}
#endif