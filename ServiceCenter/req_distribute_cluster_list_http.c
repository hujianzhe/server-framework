#include "../BootServer/global.h"
#include "../ServiceCommCode/service_comm_cmd.h"
#include "service_center_handler.h"

void reqDistributeClusterNode_http(TaskThread_t* thrd, UserMsg_t* ctrl) {
	cJSON* root, *cluster_array;
	ListNode_t* node;
	SendMsg_t msg;
	ListNode_t* cur;
	char* ret_data;
	char* reply;
	int reply_len;
	
	root = cJSON_NewObject(NULL);
	cJSON_AddNewNumber(root, "version", getClusterTableVersion());
	cluster_array = cJSON_AddNewArray(root, "clusters");
	for (node = getClusterList(ptr_g_ClusterTable())->head; node; node = node->next) {
		Cluster_t* cluster = pod_container_of(node, Cluster_t, m_listnode);
		cJSON* cjson_cluster = cJSON_AddNewObject(cluster_array, NULL);
		cJSON_AddNewString(cjson_cluster, "name", cluster->name);
		cJSON_AddNewString(cjson_cluster, "ip", cluster->ip);
		cJSON_AddNewNumber(cjson_cluster, "port", cluster->port);
		cJSON_AddNewString(cjson_cluster, "socktype", if_socktype2tring(cluster->socktype));
		cJSON_AddNewNumber(cjson_cluster, "weight_num", cluster->weight_num);
	}
	ret_data = cJSON_Print(root);
	cJSON_Delete(root);
	makeSendMsg(&msg, CMD_DISTRIBUTE_CLUSTER_LIST, ret_data, strlen(ret_data));

	for (cur = getClusterList(ptr_g_ClusterTable())->head; cur; cur = cur->next) {
		Cluster_t* cluster = pod_container_of(cur, Cluster_t, m_listnode);
		Channel_t* channel = sessionChannel(&cluster->session);
		if (!channel)
			continue;
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	}
	free(ret_data);

	reply = "{\"retcode\":0, \"desc\":\"ok\"}";
	reply = strFormat(&reply_len, HTTP_SIMPLE_RESP_FMT, HTTP_SIMPLE_RESP_VALUE(200, reply, strlen(reply)));
	channelSend(ctrl->channel, NULL, 0, NETPACKET_FRAGMENT);
	free(reply);
}