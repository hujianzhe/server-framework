#include "../BootServer/global.h"
#include "service_center_handler.h"
#include <stdio.h>

void reqClusterList_http(UserMsg_t* ctrl) {
	cJSON* root, *cluster_array;
	ListNode_t* node;
	char* ret_data, *reply;
	int reply_len;

	root = cJSON_NewObject(NULL);
	cluster_array = cJSON_AddNewArray(root, "clusters");
	for (node = ptr_g_ClusterList()->head; node; node = node->next) {
		Cluster_t* cluster = pod_container_of(node, Cluster_t, m_listnode);
		cJSON* cjson_cluster = cJSON_AddNewObject(cluster_array, NULL);
		cJSON_AddNewString(cjson_cluster, "name", cluster->name);
		cJSON_AddNewString(cjson_cluster, "ip", cluster->ip);
		cJSON_AddNewNumber(cjson_cluster, "port", cluster->port);
		cJSON_AddNewNumber(cjson_cluster, "is_online", cluster->session.channel != NULL);
	}
	ret_data = cJSON_PrintFormatted(root);
	reply = strFormat(&reply_len,
		"HTTP/1.1 %u %s\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Connection: close\r\n"
		"Content-Length:%u\r\n"
		"\r\n"
		"%s",
		200, httpframeStatusDesc(200), strlen(ret_data), ret_data
	);
	free(ret_data);
	channelSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT);
	reactorCommitCmd(NULL, &ctrl->channel->_.stream_sendfincmd);
}

void reqClusterList(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;
	cJSON *cjson_ret_root, *cjson_ret_array_cluster;
	cJSON* cjson_name, *cjson_ip, *cjson_port;
	SendMsg_t ret_msg;
	char* ret_data;
	ListNode_t* lnode;
	Cluster_t* cluster;
	int retcode = 0;

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		retcode = 1;
		goto err;
	}
	printf("req: %s\n", (char*)(ctrl->data));

	cjson_name = cJSON_Field(cjson_req_root, "name");
	if (!cjson_name) {
		retcode = 1;
		goto err;
	}
	cjson_ip = cJSON_Field(cjson_req_root, "ip");
	if (!cjson_ip) {
		retcode = 1;
		goto err;
	}
	cjson_port = cJSON_Field(cjson_req_root, "port");
	if (!cjson_port) {
		retcode = 1;
		goto err;
	}

	cluster = getCluster(cjson_name->valuestring, cjson_ip->valuestring, cjson_port->valueint);
	if (cluster) {
		Channel_t* channel = sessionUnbindChannel(&cluster->session);
		if (channel) {
			channelSendv(channel, NULL, 0, NETPACKET_FIN);
		}
	}
	else {
		retcode = 1;
		goto err;
	}
	cluster->session.id = allocSessionId();
	sessionBindChannel(&cluster->session, ctrl->channel);

	cjson_ret_root = cJSON_NewObject(NULL);
	cJSON_AddNewNumber(cjson_ret_root, "session_id", cluster->session.id);
	cjson_ret_array_cluster = cJSON_AddNewArray(cjson_ret_root, "cluster");
	for (lnode = ptr_g_ClusterList()->head; lnode; lnode = lnode->next) {
		Cluster_t* exist_cluster = pod_container_of(lnode, Cluster_t, m_listnode);
		if (exist_cluster != cluster) {
			cJSON* cjson_ret_object_cluster = cJSON_AddNewObject(cjson_ret_array_cluster, NULL);
			if (!cjson_ret_object_cluster)
				break;
			cJSON_AddNewString(cjson_ret_object_cluster, "name", exist_cluster->name);
			cJSON_AddNewString(cjson_ret_object_cluster, "ip", exist_cluster->ip);
			cJSON_AddNewNumber(cjson_ret_object_cluster, "port", exist_cluster->port);
			if (SOCK_STREAM == cluster->socktype) {
				cJSON_AddNewString(cjson_ret_object_cluster, "socktype", "SOCK_STREAM");
			}
			else {
				cJSON_AddNewString(cjson_ret_object_cluster, "socktype", "SOCK_DGRAM");
			}
		}
	}
	if (lnode) {
		cJSON_Delete(cjson_ret_root);
		retcode = 1;
		goto err;
	}
	ret_data = cJSON_Print(cjson_ret_root);
	cJSON_Delete(cjson_ret_root);

	makeSendMsgRpcResp(&ret_msg, ctrl->rpcid, 0, ret_data, strlen(ret_data));
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
	free(ret_data);
	return;
err:
	cJSON_Delete(cjson_req_root);
	makeSendMsgRpcResp(&ret_msg, ctrl->rpcid, retcode, NULL, 0);
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
}

void retClusterList(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;
	int ok;

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		fputs("cJSON_Parse", stderr);
		return;
	}
	printf("recv: %s\n", (char*)(ctrl->data));

	ok = 0;
	do {
		cJSON* cjson_session_id, *cjson_cluster_array, *cjson_cluster;

		if (!cJSON_Field(cjson_req_root, "errno")) {
			break;
		}

		cjson_session_id = cJSON_Field(cjson_req_root, "session_id");
		if (!cjson_session_id) {
			break;
		}
		cjson_cluster_array = cJSON_Field(cjson_req_root, "cluster");
		if (!cjson_cluster_array) {
			break;
		}
		for (cjson_cluster = cjson_cluster_array->child; cjson_cluster; cjson_cluster = cjson_cluster->next) {
			Cluster_t* cluster;
			cJSON* name, *socktype, *ip, *port;
			name = cJSON_Field(cjson_cluster, "name");
			if (!name)
				continue;
			socktype = cJSON_Field(cjson_cluster, "socktype");
			if (!socktype)
				continue;
			ip = cJSON_Field(cjson_cluster, "ip");
			if (!ip)
				continue;
			port = cJSON_Field(cjson_cluster, "port");
			if (!port)
				continue;
			cluster = newCluster();
			if (!cluster) {
				break;
			}
			if (!strcmp(socktype->valuestring, "SOCK_STREAM"))
				cluster->socktype = SOCK_STREAM;
			else
				cluster->socktype = SOCK_DGRAM;
			strcpy(cluster->ip, ip->valuestring);
			cluster->port = port->valueint;
			if (!regCluster(name->valuestring, cluster)) {
				freeCluster(cluster);
				break;
			}
		}
		if (cjson_cluster) {
			break;
		}
		
		channelSessionId(ctrl->channel) = cjson_session_id->valueint;
		ok = 1;
	} while (0);
	cJSON_Delete(cjson_req_root);
}

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
	else if (!strcmp(cjson_socktype->valuestring, "SOCK_STREAM"))
		socktype = SOCK_STREAM;
	else
		socktype = SOCK_DGRAM;

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