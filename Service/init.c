#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "../InnerProcHandle/inner_proc_cmd.h"
#include "../InnerProcHandle/inner_proc_cluster.h"

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "InnerProcHandle.lib")
#endif

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(int argc, char** argv) {
	ConfigConnectOption_t* option = NULL;
	unsigned int i;
	for (i = 0; i < ptr_g_Config()->connect_options_cnt; ++i) {
		option = ptr_g_Config()->connect_options + i;
		if (!strcmp(option->protocol, "ServiceCenter"))
			break;
	}
	if (!option) {
		logErr(ptr_g_Log(), "miss connect service config");
		return 0;
	}
	if (!regNumberDispatch(CMD_RET_CLUSTER_LIST, retClusterList)) {
		logErr(ptr_g_Log(), "regNumberDispatch(CMD_RET_CLUSTER_LIST, retClusterList) failure");
		return 0;
	}
	if (!callReqClusterList(ipstrFamily(option->ip), option->ip, option->port)) {
		logErr(ptr_g_Log(), "req_cluster_list failure");
		return 0;
	}
	return 1;
}

__declspec_dllexport void destroy(void) {

}

#ifdef __cplusplus
}
#endif