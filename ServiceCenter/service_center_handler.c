#include "../BootServer/global.h"
#include "service_center_handler.h"
#include <stdio.h>

int reqClusterList_http(UserMsg_t* ctrl) {
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
	return 1;
}

int reqClusterList(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;
	cJSON *cjson_ret_root, *cjson_ret_array_cluster;
	SendMsg_t ret_msg;
	char* ret_data;
	ListNode_t* lnode;
	Cluster_t* cluster;
	int ok;

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		fputs("cJSON_Parse", stderr);
		return 0;
	}
	printf("req: %s\n", (char*)(ctrl->data));

	ok = 0;
	do {
		cJSON* cjson_name, *cjson_ip, *cjson_port;

		cjson_name = cJSON_Field(cjson_req_root, "name");
		if (!cjson_name) {
			break;
		}
		cjson_ip = cJSON_Field(cjson_req_root, "ip");
		if (!cjson_ip) {
			break;
		}
		cjson_port = cJSON_Field(cjson_req_root, "port");
		if (!cjson_port) {
			break;
		}

		cluster = getCluster(cjson_name->valuestring, cjson_ip->valuestring, cjson_port->valueint);
		if (cluster) {
			Channel_t* channel = sessionUnbindChannel(&cluster->session);
			if (channel) {
				channelSendv(channel, NULL, 0, NETPACKET_FIN);
			}
		}
		else {
			break;
		}
		cluster->session.id = allocSessionId();
		sessionBindChannel(&cluster->session, ctrl->channel);
		ok = 1;
	} while (0);
	cJSON_Delete(cjson_req_root);
	if (!ok) {
		const char ret_data[] = "{\"errno\":1}";
		makeSendMsg(&ret_msg, CMD_RET_CLUSTER_LIST, ret_data, sizeof(ret_data) - 1);
		channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
		return 0;
	}

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
		}
	}
	if (lnode) {
		cJSON_Delete(cjson_ret_root);
		return 0;
	}
	ret_data = cJSON_Print(cjson_ret_root);
	cJSON_Delete(cjson_ret_root);

	makeSendMsg(&ret_msg, CMD_RET_CLUSTER_LIST, ret_data, strlen(ret_data));
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
	free(ret_data);
	return 0;
}

int reqClusterLogin(UserMsg_t* ctrl) {
	ListNode_t* cluster_listnode;
	Cluster_t* cluster;
	Session_t* session;

	session = channelSession(ctrl->channel);
	if (!session)
		return 0;
	cluster = pod_container_of(session, Cluster_t, session);

	for (cluster_listnode = ptr_g_ClusterList()->head; cluster_listnode; cluster_listnode = cluster_listnode->next) {
		Cluster_t* exist_cluster = pod_container_of(cluster_listnode, Cluster_t, m_listnode);
		if (!exist_cluster->session.channel) {
			continue;
		}
		// TODO notify other online cluster
	}

	return 0;
}