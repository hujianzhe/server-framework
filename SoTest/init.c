#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "mq_cmd.h"
#include "mq_handler.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(int argc, char** argv) {
	int connectsockinitokcnt;

	set_g_DefaultDispatchCallback(unknowRequest);
	regNumberDispatch(CMD_REQ_TEST, reqTest);
	regNumberDispatch(CMD_NOTIFY_TEST, notifyTest);
	regNumberDispatch(CMD_RET_TEST, retTest);
	regNumberDispatch(CMD_REQ_RECONNECT, reqReconnectCluster);
	regNumberDispatch(CMD_RET_RECONNECT, retReconnect);
	regNumberDispatch(CMD_REQ_UPLOAD_CLUSTER, reqUploadCluster);
	regNumberDispatch(CMD_RET_UPLOAD_CLUSTER, retUploadCluster);
	regNumberDispatch(CMD_NOTIFY_NEW_CLUSTER, notifyNewCluster);
	regNumberDispatch(CMD_REQ_REMOVE_CLUSTER, reqRemoveCluster);
	regNumberDispatch(CMD_RET_REMOVE_CLUSTER, retRemoveCluster);
	regStringDispatch("/reqHttpTest", reqHttpTest);
	regStringDispatch("/reqSoTest", reqSoTest);

	for (connectsockinitokcnt = 0; connectsockinitokcnt < ptr_g_Config()->connect_options_cnt; ++connectsockinitokcnt) {
		ConfigConnectOption_t* option = ptr_g_Config()->connect_options + connectsockinitokcnt;
		RpcItem_t* rpc_item;
		Sockaddr_t connect_addr;
		Channel_t* c;
		ReactorObject_t* o;
		if (strcmp(option->protocol, "inner")) {
			continue;
		}
		if (!sockaddrEncode(&connect_addr.st, ipstrFamily(option->ip), option->ip, option->port))
			return 0;
		o = reactorobjectOpen(INVALID_FD_HANDLE, connect_addr.st.ss_family, option->socktype, 0);
		if (!o)
			return 0;
		c = openChannel(o, CHANNEL_FLAG_CLIENT, &connect_addr);
		if (!c) {
			reactorCommitCmd(NULL, &o->freecmd);
			return 0;
		}
		c->_.on_syn_ack = defaultRpcOnSynAck;
		c->on_heartbeat = defaultOnHeartbeat;
		printf("channel(%p) connecting......\n", c);
		if (!newRpcItemFiberReady(ptr_g_RpcFiberCore(), c, 5000)) {
			return 1;
		}
		reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);
		rpc_item = rpcFiberCoreYield(ptr_g_RpcFiberCore());
		if (rpc_item->ret_msg) {
			SendMsg_t msg;
			char* req_data;
			int req_datalen;
			req_data = strFormat(&req_datalen, "{\"name\":\"%s\",\"ip\":\"%s\",\"port\":%u}",
				ptr_g_ClusterSelf()->name, ptr_g_ClusterSelf()->ip, ptr_g_ClusterSelf()->port);
			if (!req_data) {
				return 0;
			}
			makeSendMsg(&msg, CMD_REQ_UPLOAD_CLUSTER, req_data, req_datalen);
			channelSendv(c, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
			free(req_data);
		}
		else {
			printf("channel(%p) connect %s:%u failure\n", c, option->ip, option->port);
			return 0;
		}
	}

	return 1;
}

__declspec_dllexport void destroy(void) {

}

#ifdef __cplusplus
}
#endif