#include "../BootServer/global.h"
#include "../ServiceCommCode/service_comm_cmd.h"
#include "service_center_handler.h"

void reqClusterList(TaskThread_t* thrd, UserMsg_t* ctrl) {
	cJSON* cjson_req_root;
	cJSON *cjson_ret_root, *cjson_ret_array_cluster;
	cJSON *cjson_ip, *cjson_port, *cjson_socktype;
	SendMsg_t ret_msg;
	char* ret_data;
	ListNode_t* lnode;
	int retcode = 0;

	logInfo(ptr_g_Log(), "%s req: %s", __FUNCTION__, (char*)(ctrl->data));

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		logErr(ptr_g_Log(), "%s: cJSON_Parse err", __FUNCTION__);
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
	cjson_socktype = cJSON_Field(cjson_req_root, "socktype");
	if (!cjson_socktype) {
		retcode = 1;
		goto err;
	}

	cjson_ret_root = cJSON_NewObject(NULL);
	cJSON_AddNewNumber(cjson_ret_root, "version", getClusterTableVersion());
	cjson_ret_array_cluster = cJSON_AddNewArray(cjson_ret_root, "cluster_nodes");
	for (lnode = getClusterNodeList(ptr_g_ClusterTable())->head; lnode; lnode = lnode->next) {
		cJSON* cjson_hashkey_arr;
		ClusterNode_t* clsnd = pod_container_of(lnode, ClusterNode_t, m_listnode);
		cJSON* cjson_ret_object_cluster = cJSON_AddNewObject(cjson_ret_array_cluster, NULL);
		if (!cjson_ret_object_cluster)
			break;
		cJSON_AddNewString(cjson_ret_object_cluster, "name", clsnd->name);
		cJSON_AddNewString(cjson_ret_object_cluster, "ip", clsnd->ip);
		cJSON_AddNewNumber(cjson_ret_object_cluster, "port", clsnd->port);
		cJSON_AddNewString(cjson_ret_object_cluster, "socktype", if_socktype2string(clsnd->socktype));
		cJSON_AddNewNumber(cjson_ret_object_cluster, "weight_num", clsnd->weight_num);
		cjson_hashkey_arr = cJSON_AddNewArray(cjson_ret_object_cluster, "hash_key");
		if (cjson_hashkey_arr) {
			unsigned int i;
			for (i = 0; i < clsnd->hashkey_cnt; ++i) {
				cJSON_AddNewNumber(cjson_hashkey_arr, NULL, clsnd->hashkey[i]);
			}
		}
		if (!strcmp(clsnd->ip, cjson_ip->valuestring) &&
			clsnd->port == cjson_port->valueint &&
			clsnd->socktype == if_string2socktype(cjson_socktype->valuestring))
		{
			sessionChannelReplaceServer(&clsnd->session, ctrl->channel);
		}
	}
	if (lnode) {
		cJSON_Delete(cjson_ret_root);
		retcode = 1;
		goto err;
	}
	ret_data = cJSON_Print(cjson_ret_root);
	cJSON_Delete(cjson_ret_root);

	if (ctrl->rpc_status == 'R') {
		makeSendMsgRpcResp(&ret_msg, ctrl->rpcid, 0, ret_data, strlen(ret_data));
	}
	else {
		makeSendMsg(&ret_msg, CMD_RET_CLUSTER_LIST, ret_data, strlen(ret_data));
	}
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
	free(ret_data);
	return;
err:
	cJSON_Delete(cjson_req_root);
	if (ctrl->rpc_status == 'R') {
		makeSendMsgRpcResp(&ret_msg, ctrl->rpcid, retcode, NULL, 0);
		ret_data = NULL;
	}
	else {
		int ret_datalen;
		ret_data = strFormat(&ret_datalen, "{\"errno\":%d}", retcode);
		if (!ret_data) {
			return;
		}
		makeSendMsg(&ret_msg, CMD_RET_CLUSTER_LIST, ret_data, ret_datalen);
	}
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
	free(ret_data);
}