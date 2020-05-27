#include "../BootServer/global.h"
#include "service_center_handler.h"

void reqDistributeClusterNode_http(UserMsg_t* ctrl) {
	SendMsg_t msg;
	ListNode_t* cur;
	for (cur = getClusterList(ptr_g_ClusterTable())->head; cur; cur = cur->next) {
		Cluster_t* cluster = pod_container_of(cur, Cluster_t, m_listnode);
		Channel_t* channel = sessionChannel(&cluster->session);
		if (!channel)
			continue;
		// TODO
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	}
}