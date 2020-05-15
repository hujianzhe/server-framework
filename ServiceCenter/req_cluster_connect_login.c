#include "../BootServer/global.h"
#include "service_center_handler.h"
#include <stdio.h>

void reqClusterConnectLogin(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;
	UserMsg_t dup_ctrl = *ctrl;
	SendMsg_t msg;
	cJSON* ip, *port, *cjson_socktype;
	int socktype;
	ReactorObject_t* o;
	Channel_t* c;
	Sockaddr_t connect_addr;
	RpcItem_t* rpc_item;
	int retcode = 0;
	char* req_data;
	int req_datalen;

	cjson_req_root = cJSON_Parse(NULL, ctrl->data);
	if (!cjson_req_root) {
		retcode = 1;
		goto err;
	}

	ip = cJSON_Field(cjson_req_root, "ip");
	if (!ip) {
		retcode = 1;
		goto err;
	}
	port = cJSON_Field(cjson_req_root, "port");
	if (!port) {
		retcode = 1;
		goto err;
	}
	cjson_socktype = cJSON_Field(cjson_req_root, "socktype");
	if (!cjson_socktype) {
		retcode = 1;
		goto err;
	}
	socktype = if_string2socktype(cjson_socktype->valuestring);

	cJSON_Delete(cjson_req_root);

	if (!sockaddrEncode(&connect_addr.st, ipstrFamily(ip->valuestring), ip->valuestring, port->valueint)) {
		retcode = 1;
		goto err;
	}
	o = reactorobjectOpen(INVALID_FD_HANDLE, connect_addr.st.ss_family, socktype, 0);
	if (!o) {
		retcode = 1;
		goto err;
	}
	c = openChannel(o, CHANNEL_FLAG_CLIENT, &connect_addr);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		retcode = 1;
		goto err;
	}
	c->_.on_syn_ack = defaultRpcOnSynAck;
	c->on_heartbeat = defaultOnHeartbeat;

	rpc_item = newRpcItemFiberReady(ptr_g_RpcFiberCore(), c, 500);
	if (!rpc_item) {
		reactorCommitCmd(NULL, &o->freecmd);
		reactorCommitCmd(NULL, &c->_.freecmd);
		retcode = 1;
		goto err;
	}
	reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);

	_xadd32(&dup_ctrl.channel->_.refcnt, 1);
	rpc_item = rpcFiberCoreYield(ptr_g_RpcFiberCore());
	if (!rpc_item->ret_msg) {
		retcode = 1;
		goto end;
	}
	ctrl = (UserMsg_t*)rpc_item->ret_msg;

	req_data = strFormat(&req_datalen, "{\"name\":\"%s\",\"ip\":\"%s\",\"port\":%u}",
		ptr_g_ClusterSelf()->name, ptr_g_ClusterSelf()->ip, ptr_g_ClusterSelf()->port);
	if (!req_data) {
		retcode = 1;
		goto end;
	}
	rpc_item = newRpcItemFiberReady(ptr_g_RpcFiberCore(), ctrl->channel, 500);
	if (!rpc_item) {
		free(req_data);
		retcode = 1;
		goto end;
	}
	makeSendMsgRpcReq(&msg, rpc_item->id, CMD_REQ_CLUSTER_TELL_SELF, req_data, req_datalen);
	channelSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	free(req_data);
	rpc_item = rpcFiberCoreYield(ptr_g_RpcFiberCore());
	if (!rpc_item->ret_msg) {
		retcode = 1;
	}
	else {
		retcode = ((UserMsg_t*)rpc_item->ret_msg)->retcode;
	}
end:
	makeSendMsgRpcResp(&msg, dup_ctrl.rpcid, retcode, NULL, 0);
	channelSendv(dup_ctrl.channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	reactorCommitCmd(NULL, &dup_ctrl.channel->_.freecmd);
	return;
err:
	cJSON_Delete(cjson_req_root);
	makeSendMsgRpcResp(&msg, dup_ctrl.rpcid, retcode, NULL, 0);
	channelSendv(dup_ctrl.channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
}