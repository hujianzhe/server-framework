#include "inner_proc_cluster.h"
#include "inner_proc_cmd.h"

static int ret_cluster_list(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;
	cJSON* cjson_cluster_array, *cjson_cluster, *cjson_version;
	int cluster_self_find;

	logInfo(ptr_g_Log(), "%s recv: %s", __FUNCTION__, (char*)(ctrl->data));

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		logErr(ptr_g_Log(), "cJSON_Parse error");
		return 0;
	}	

	if (cJSON_Field(cjson_req_root, "errno"))
		goto err;

	cjson_version = cJSON_Field(cjson_req_root, "version");
	if (!cjson_version) {
		goto err;
	}
	cjson_cluster_array = cJSON_Field(cjson_req_root, "clusters");
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
		if (!strcmp(if_socktype2tring(getClusterSelf()->socktype), socktype->valuestring) &&
			!strcmp(getClusterSelf()->ip, ip->valuestring) &&
			getClusterSelf()->port == port->valueint)
		{
			cluster = getClusterSelf();
			cluster_self_find = 1;
		}
		else {
			cluster = newCluster(if_string2socktype(socktype->valuestring), ip->valuestring, port->valueint);
			if (!cluster) {
				break;
			}
		}
		if (!regCluster(ptr_g_ClusterTable(), name->valuestring, cluster)) {
			freeCluster(cluster);
			break;
		}
	}
	if (cjson_cluster) {
		goto err;
	}
	if (!cluster_self_find)
		goto err;
	setClusterTableVersion(cjson_version->valueint);
	cJSON_Delete(cjson_req_root);

	if (getClusterSelf()->port) {
		ReactorObject_t* o = openListener(getClusterSelf()->socktype, getClusterSelf()->ip, getClusterSelf()->port);
		if (!o)
			return 0;
		reactorCommitCmd(ptr_g_ReactorAccept(), &o->regcmd);
	}

	return 1;
err:
	cJSON_Delete(cjson_req_root);
	return 0;
}

static void rpc_ret_cluster_list(RpcAsyncCore_t* rpc, RpcItem_t* rpc_item) {
	if (rpc_item->ret_msg) {
		UserMsg_t* ctrl = (UserMsg_t*)rpc_item->ret_msg;
		if (0 == ctrl->retcode && ret_cluster_list(ctrl)) {
			return;
		}
	}
	g_Invalid();
}

static void retClusterList(TaskThread_t* thrd, UserMsg_t* ctrl) {
	ret_cluster_list(ctrl);
}

static int start_req_cluster_list(TaskThread_t* thrd, Channel_t* channel) {
	SendMsg_t msg;
	char* req_data;
	int req_datalen;
	req_data = strFormat(&req_datalen, "{\"ip\":\"%s\",\"port\":%u}",
		getClusterSelf()->ip, getClusterSelf()->port);
	if (!req_data) {
		return 0;
	}
	if (!thrd->f_rpc && !thrd->a_rpc) {
		makeSendMsg(&msg, CMD_REQ_CLUSTER_LIST, req_data, req_datalen);
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		free(req_data);
	}
	else {
		RpcItem_t* rpc_item;
		if (thrd->f_rpc) {
			rpc_item = newRpcItemFiberReady(thrd, channel, 5000);
			if (!rpc_item)
				goto err;
		}
		else {
			rpc_item = newRpcItemAsyncReady(thrd, channel, 5000, NULL, rpc_ret_cluster_list);
			if (!rpc_item)
				goto err;
		}
		makeSendMsgRpcReq(&msg, rpc_item->id, CMD_REQ_CLUSTER_LIST, req_data, req_datalen);
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		free(req_data);
		if (thrd->a_rpc)
			return 1;
		rpc_item = rpcFiberCoreYield(thrd->f_rpc);
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

int rpcReqClusterList(TaskThread_t* thrd, Cluster_t* sc_cluster) {
	Sockaddr_t connect_addr;
	ReactorObject_t* o;
	Channel_t* c;
	SendMsg_t msg;
	char* req_data;
	int req_datalen;
	int domain;

	if (!regNumberDispatch(CMD_RET_CLUSTER_LIST, retClusterList)) {
		logErr(ptr_g_Log(), "regNumberDispatch(CMD_RET_CLUSTER_LIST, retClusterList) failure");
		return 0;
	}
	req_data = strFormat(&req_datalen, "{\"ip\":\"%s\",\"port\":%u}",
		getClusterSelf()->ip, getClusterSelf()->port);
	if (!req_data) {
		return 0;
	}


	domain = ipstrFamily(sc_cluster->ip);
	if (!sockaddrEncode(&connect_addr.st, domain, sc_cluster->ip, sc_cluster->port))
		return 0;
	o = reactorobjectOpen(INVALID_FD_HANDLE, connect_addr.st.ss_family, sc_cluster->socktype, 0);
	if (!o)
		return 0;
	c = openChannel(o, CHANNEL_FLAG_CLIENT, &connect_addr);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		return 0;
	}
	sessionChannelReplaceClient(&sc_cluster->session, c);
	reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);

	logInfo(ptr_g_Log(), "channel connecting ServiceCenter, ip:%s, port:%u, and ReqClusterList ......",
		sc_cluster->ip, sc_cluster->port);

	if (!thrd->f_rpc && !thrd->a_rpc) {
		makeSendMsg(&msg, CMD_REQ_CLUSTER_LIST, req_data, req_datalen);
		channelSendv(c, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		free(req_data);
	}
	else {
		RpcItem_t* rpc_item;
		if (thrd->f_rpc) {
			rpc_item = newRpcItemFiberReady(thrd, c, 5000);
			if (!rpc_item) {
				free(req_data);
				return 0;
			}
		}
		else {
			rpc_item = newRpcItemAsyncReady(thrd, c, 5000, NULL, rpc_ret_cluster_list);
			if (!rpc_item) {
				free(req_data);
				return 0;
			}
		}
		makeSendMsgRpcReq(&msg, rpc_item->id, CMD_REQ_CLUSTER_LIST, req_data, req_datalen);
		channelSendv(c, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		free(req_data);
		if (!thrd->f_rpc)
			return 1;
		rpc_item = rpcFiberCoreYield(thrd->f_rpc);
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
}

#ifdef __cplusplus
}
#endif