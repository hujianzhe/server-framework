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

	for (cur = getClusterNodeList(ptr_g_ClusterTable())->head; cur; cur = cur->next) {
		ClusterNode_t* old_clsnd = pod_container_of(cur, ClusterNode_t, m_listnode);
		ClusterNode_t* new_clsnd = getClusterNode(table, old_clsnd->socktype, old_clsnd->ip, old_clsnd->port);
		if (new_clsnd) {
			Channel_t* client_channel, *server_channel;
			client_channel = old_clsnd->session.channel_client;
			server_channel = old_clsnd->session.channel_server;
			sessionUnbindChannel(&old_clsnd->session);
			sessionChannelReplaceClient(&new_clsnd->session, client_channel);
			sessionChannelReplaceServer(&new_clsnd->session, server_channel);
		}
		if (getClusterNodeSelf() == old_clsnd) {
			if (new_clsnd) {
				new_clsnd->weight_num = old_clsnd->weight_num;
				new_clsnd->connection_num = old_clsnd->connection_num;
				setClusterNodeSelf(new_clsnd);
			}
			else
				unregClusterNode(ptr_g_ClusterTable(), old_clsnd);
		}
	}
	freeClusterTable(ptr_g_ClusterTable());
	set_g_ClusterTable(table);
}

#ifdef __cplusplus
}
#endif