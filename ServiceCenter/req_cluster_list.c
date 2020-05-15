#include "../BootServer/global.h"
#include "service_center_handler.h"
#include <stdio.h>

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
		cJSON* cjson_ret_object_cluster = cJSON_AddNewObject(cjson_ret_array_cluster, NULL);
		if (!cjson_ret_object_cluster)
			break;
		cJSON_AddNewString(cjson_ret_object_cluster, "name", exist_cluster->name);
		cJSON_AddNewString(cjson_ret_object_cluster, "ip", exist_cluster->ip);
		cJSON_AddNewNumber(cjson_ret_object_cluster, "port", exist_cluster->port);
		cJSON_AddNewString(cjson_ret_object_cluster, "socktype", if_socktype2tring(cluster->socktype));
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