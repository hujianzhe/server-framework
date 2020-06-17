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
	for (node = getClusterNodeList(ptr_g_ClusterTable())->head; node; node = node->next) {
		cJSON* cjson_hashkey_arr;
		ClusterNode_t* clsnd = pod_container_of(node, ClusterNode_t, m_listnode);
		cJSON* cjson_cluster = cJSON_AddNewObject(cluster_array, NULL);
		cJSON_AddNewString(cjson_cluster, "name", clsnd->name);
		cJSON_AddNewString(cjson_cluster, "ip", clsnd->ip);
		cJSON_AddNewNumber(cjson_cluster, "port", clsnd->port);
		cJSON_AddNewNumber(cjson_cluster, "weight_num", clsnd->weight_num);
		cJSON_AddNewNumber(cjson_cluster, "is_online", sessionChannel(&clsnd->session) != NULL);
		cjson_hashkey_arr = cJSON_AddNewArray(cjson_cluster, "hash_key");
		if (cjson_hashkey_arr) {
			unsigned int i;
			for (i = 0; i < clsnd->hashkey_cnt; ++i) {
				cJSON_AddNewNumber(cjson_hashkey_arr, NULL, clsnd->hashkey[i]);
			}
		}
	}
	ret_data = cJSON_PrintFormatted(root);
	reply = strFormat(&reply_len, HTTP_SIMPLE_RESP_FMT, HTTP_SIMPLE_RESP_VALUE(200, ret_data, strlen(ret_data)));
	free(ret_data);
	channelSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT);
	reactorCommitCmd(NULL, &ctrl->channel->_.stream_sendfincmd);
}