#include "../BootServer/global.h"
#include "service_center_handler.h"
#include <stdio.h>

void reqClusterList_http(TaskThread_t* thrd, UserMsg_t* ctrl) {
	cJSON* root, *cluster_array;
	ListNode_t* node;
	char* ret_data, *reply;
	int reply_len;

	logInfo(ptr_g_Log(), "%s query:%s, data:%s", __FUNCTION__, ctrl->httpframe->query, (char*)ctrl->data);

	root = cJSON_NewObject(NULL);
	cJSON_AddNewNumber(root, "version", getClusterTableVersion());
	cluster_array = cJSON_AddNewArray(root, "clusters");
	for (node = getClusterList(ptr_g_ClusterTable())->head; node; node = node->next) {
		Cluster_t* cluster = pod_container_of(node, Cluster_t, m_listnode);
		cJSON* cjson_cluster = cJSON_AddNewObject(cluster_array, NULL);
		cJSON_AddNewString(cjson_cluster, "name", cluster->name);
		cJSON_AddNewString(cjson_cluster, "ip", cluster->ip);
		cJSON_AddNewNumber(cjson_cluster, "port", cluster->port);
		cJSON_AddNewNumber(cjson_cluster, "is_online", sessionChannel(&cluster->session) != NULL);
	}
	ret_data = cJSON_PrintFormatted(root);
	reply = strFormat(&reply_len, HTTP_SIMPLE_RESP_FMT, HTTP_SIMPLE_RESP_VALUE(200, ret_data, strlen(ret_data)));
	free(ret_data);
	channelSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT);
	reactorCommitCmd(NULL, &ctrl->channel->_.stream_sendfincmd);
}