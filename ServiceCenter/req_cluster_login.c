#include "../BootServer/global.h"
#include "service_center_handler.h"
#include <stdio.h>

void reqClusterLogin(UserMsg_t* ctrl) {
	ListNode_t* cluster_listnode;
	Cluster_t* cluster;
	Session_t* session;
	char* req_data;
	int req_datalen;
	int cnt, retcode = 0;
	RpcItem_t* rpc_item;
	SendMsg_t msg;
	UserMsg_t dup_ctrl = *ctrl;

	session = channelSession(ctrl->channel);
	if (!session) {
		retcode = 1;
		goto err;
	}
	cluster = pod_container_of(session, Cluster_t, session);

	req_data = strFormat(&req_datalen, "{\"name\":\"%s\",\"ip\":\"%s\",\"port\":%u}", cluster->name, cluster->ip, cluster->port);
	if (!req_data) {
		retcode = 1;
		goto err;
	}
	cnt = 0;
	for (cluster_listnode = ptr_g_ClusterList()->head; cluster_listnode; cluster_listnode = cluster_listnode->next) {
		Cluster_t* exist_cluster = pod_container_of(cluster_listnode, Cluster_t, m_listnode);
		Channel_t* channel = exist_cluster->session.channel;
		if (channel) {
			continue;
		}
		rpc_item = newRpcItemFiberReady(ptr_g_RpcFiberCore(), channel, 5000);
		if (!rpc_item) {
			retcode = 1;
			goto err;
		}
		makeSendMsgRpcReq(&msg, rpc_item->id, CMD_REQ_CLUSTER_CONNECT_LOGIN, req_data, req_datalen);
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		++cnt;
	}
	free(req_data);
	_xadd32(&dup_ctrl.channel->_.refcnt, 1);
	while (cnt--) {
		rpc_item = rpcFiberCoreYield(ptr_g_RpcFiberCore());
		if (!rpc_item->ret_msg) {
			retcode = 1;
		}
		else {
			UserMsg_t* ctrl = (UserMsg_t*)rpc_item->ret_msg;
			if (ctrl->retcode != 0) {
				retcode = ctrl->retcode;
			}
		}
	}
	makeSendMsgRpcResp(&msg, dup_ctrl.rpcid, retcode, NULL, 0);
	channelSendv(dup_ctrl.channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	reactorCommitCmd(NULL, &dup_ctrl.channel->_.freecmd);
	return;
err:
	free(req_data);
	makeSendMsgRpcResp(&msg, dup_ctrl.rpcid, retcode, NULL, 0);
	channelSendv(dup_ctrl.channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
}