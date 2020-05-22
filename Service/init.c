#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "../InnerProcHandle/inner_proc_cmd.h"
#include "../InnerProcHandle/inner_proc_cluster.h"

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "InnerProcHandle.lib")
#endif

static int service_center_check_connection_timeout_callback(RBTimer_t* timer, RBTimerEvent_t* e) {
	ClusterGroup_t* sc_grp;
	Cluster_t* sc_cluster;
	sc_grp = getClusterGroup("ServiceCenter");
	sc_cluster = pod_container_of(sc_grp->clusterlist.head, Cluster_t, m_grp_listnode);
	clusterChannel(sc_cluster);

	logInfo(ptr_g_Log(), "__FUNCTION__");

	e->timestamp_msec = gmtimeMillisecond() + 1000 * 60;
	rbtimerAddEvent(timer, e);
	return 1;
}

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(int argc, char** argv) {
	ConfigConnectOption_t* option = NULL;
	Cluster_t* cluster;
	RBTimerEvent_t* timeout_ev;
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
	cluster = newCluster();
	if (!cluster) {
		logErr(ptr_g_Log(), "ServiceCenter newCluster error");
		return 0;
	}
	cluster->socktype = option->socktype;
	strcpy(cluster->ip, option->ip);
	cluster->port = option->port;
	if (!regCluster(option->protocol, cluster)) {
		logErr(ptr_g_Log(), "ServiceCenter regCluster error");
		return 0;
	}
	if (!regNumberDispatch(CMD_RET_CLUSTER_LIST, retClusterList)) {
		logErr(ptr_g_Log(), "regNumberDispatch(CMD_RET_CLUSTER_LIST, retClusterList) failure");
		return 0;
	}
	if (!callReqClusterList(cluster)) {
		logErr(ptr_g_Log(), "req_cluster_list failure");
		return 0;
	}
	timeout_ev = (RBTimerEvent_t*)malloc(sizeof(RBTimerEvent_t));
	if (!timeout_ev) {
		logErr(ptr_g_Log(), "malloc(sizeof(RBTimerEvent_t)) error");
		return 0;
	}
	timeout_ev->arg = NULL;
	timeout_ev->timestamp_msec = gmtimeMillisecond() + 1000 * 60;
	timeout_ev->callback = service_center_check_connection_timeout_callback;
	rbtimerAddEvent(ptr_g_Timer(), timeout_ev);
	return 1;
}

__declspec_dllexport void destroy(void) {

}

#ifdef __cplusplus
}
#endif