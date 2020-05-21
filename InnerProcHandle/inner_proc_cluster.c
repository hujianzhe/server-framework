#include "inner_proc_cluster.h"
#include "inner_proc_cmd.h"

static int ret_cluster_list(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;
	cJSON* cjson_cluster_array, *cjson_cluster;
	int cluster_self_find;

	logInfo(ptr_g_Log(), "recv: %s\n", (char*)(ctrl->data));

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		logErr(ptr_g_Log(), "cJSON_Parse error");
		return 0;
	}	

	if (cJSON_Field(cjson_req_root, "errno"))
		goto err;

	cjson_cluster_array = cJSON_Field(cjson_req_root, "cluster");
	if (!cjson_cluster_array) {
		goto err;
	}
	cluster_self_find = 0;
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
		if (!strcmp(if_socktype2tring(ptr_g_ClusterSelf()->socktype), socktype->valuestring) &&
			!strcmp(ptr_g_ClusterSelf()->ip, ip->valuestring) &&
			ptr_g_ClusterSelf()->port == port->valueint)
		{
			cluster = ptr_g_ClusterSelf();
			cluster_self_find = 1;
		}
		else {
			cluster = newCluster();
			if (!cluster) {
				break;
			}
			cluster->socktype = if_string2socktype(socktype->valuestring);
			strcpy(cluster->ip, ip->valuestring);
			cluster->port = port->valueint;
		}
		if (!regCluster(name->valuestring, cluster)) {
			freeCluster(cluster);
			break;
		}
	}
	if (cjson_cluster) {
		goto err;
	}
	if (!cluster_self_find)
		goto err;
	cJSON_Delete(cjson_req_root);
	return 1;
err:
	cJSON_Delete(cjson_req_root);
	return 0;
}

static void rpc_ret_cluster_list(RpcItem_t* rpc_item) {
	if (rpc_item->ret_msg) {
		UserMsg_t* ctrl = (UserMsg_t*)rpc_item->ret_msg;
		if (0 == ctrl->retcode && ret_cluster_list(ctrl)) {
			return;
		}
	}
	g_Invalid();
}

static int start_req_cluster_list(Channel_t* channel) {
	SendMsg_t msg;
	char* req_data;
	int req_datalen;
	req_data = strFormat(&req_datalen, "{\"ip\":\"%s\",\"port\":%u}",
		ptr_g_ClusterSelf()->ip, ptr_g_ClusterSelf()->port);
	if (!req_data) {
		return 0;
	}
	if (!ptr_g_RpcFiberCore() && !ptr_g_RpcAsyncCore()) {
		makeSendMsg(&msg, CMD_REQ_CLUSTER_LIST, req_data, req_datalen);
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		free(req_data);
	}
	else {
		RpcItem_t* rpc_item;
		if (ptr_g_RpcFiberCore()) {
			rpc_item = newRpcItemFiberReady(ptr_g_RpcFiberCore(), channel, 5000);
			if (!rpc_item)
				goto err;
		}
		else {
			rpc_item = newRpcItemAsyncReady(ptr_g_RpcAsyncCore(), channel, 5000, NULL, rpc_ret_cluster_list);
			if (!rpc_item)
				goto err;
		}
		makeSendMsgRpcReq(&msg, rpc_item->id, CMD_REQ_CLUSTER_LIST, req_data, req_datalen);
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		free(req_data);
		if (ptr_g_RpcAsyncCore())
			return 1;
		rpc_item = rpcFiberCoreYield(ptr_g_RpcFiberCore());
		if (rpc_item->ret_msg) {
			UserMsg_t* ctrl = (UserMsg_t*)rpc_item->ret_msg;
			if (ctrl->retcode)
				return 0;
			if (!ret_cluster_list(ctrl))
				return 0;
		}
		else {
			return 0;
		}
	}
	return 1;
err:
	free(req_data);
	return 0;
}

#ifdef __cplusplus
extern "C" {
#endif

int callReqClusterList(int socktype, const char* ip, unsigned short port) {
	Sockaddr_t connect_addr;
	ReactorObject_t* o;
	Channel_t* c;
	int domain = ipstrFamily(ip);

	if (!sockaddrEncode(&connect_addr.st, domain, ip, port))
		return 0;
	o = reactorobjectOpen(INVALID_FD_HANDLE, connect_addr.st.ss_family, socktype, 0);
	if (!o)
		return 0;
	c = openChannel(o, CHANNEL_FLAG_CLIENT, &connect_addr);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		return 0;
	}
	c->on_heartbeat = defaultOnHeartbeat;
	c->_.on_syn_ack = defaultOnSynAck;
	reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);
	logInfo(ptr_g_Log(), "channel(%p) connecting ServiceCenter, ip:%s, port:%u ......\n", c, ip, port);
	if (!start_req_cluster_list(c)) {
		logErr(ptr_g_Log(), "start_req_cluster_list failure, ip:%s, port:%u ......\n", ip, port);
		return 0;
	}
	return 1;
}

void retClusterList(UserMsg_t* ctrl) {
	ret_cluster_list(ctrl);
}

#ifdef __cplusplus
}
#endif