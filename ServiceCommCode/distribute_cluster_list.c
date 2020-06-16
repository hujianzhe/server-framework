#include "service_comm_cmd.h"
#include "service_comm_proc.h"

#ifdef __cplusplus
extern "C" {
#endif

void distributeClusterList(TaskThread_t* thrd, UserMsg_t* ctrl) {
	struct ClusterTable_t* table;
	ListNode_t* cur;

	table = newClusterTable();
	if (!table) {
		logErr(ptr_g_Log(), "newClusterTable error");
		return;
	}

	if (!loadClusterNodeFromJsonData(table, (char*)ctrl->data)) {
		logErr(ptr_g_Log(), "%s.loadClusterNodeFromJsonData error", __FUNCTION__);
		freeClusterTable(table);
		return;
	}

	for (cur = getClusterList(ptr_g_ClusterTable())->head; cur; cur = cur->next) {
		Cluster_t* old_cluster = pod_container_of(cur, Cluster_t, m_listnode);
		Cluster_t* new_cluster = getCluster(table, old_cluster->name, old_cluster->ip, old_cluster->port);
		if (new_cluster) {
			Channel_t* client_channel, *server_channel;
			client_channel = old_cluster->session.channel_client;
			server_channel = old_cluster->session.channel_server;
			sessionUnbindChannel(&old_cluster->session);
			sessionChannelReplaceClient(&new_cluster->session, client_channel);
			sessionChannelReplaceServer(&new_cluster->session, server_channel);
		}
		if (getClusterSelf() == old_cluster) {
			if (new_cluster) {
				new_cluster->weight_num = old_cluster->weight_num;
				new_cluster->connection_num = old_cluster->connection_num;
				setClusterSelf(new_cluster);
			}
			else
				unregCluster(ptr_g_ClusterTable(), old_cluster);
		}
	}
	freeClusterTable(ptr_g_ClusterTable());
	set_g_ClusterTable(table);
}

#ifdef __cplusplus
}
#endif