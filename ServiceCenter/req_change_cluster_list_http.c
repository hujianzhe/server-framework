#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "service_center_handler.h"

void reqChangeClusterNode_http(TaskThread_t* thrd, UserMsg_t* ctrl) {
	cJSON* cjson_cluster_array, *cjson_cluster;
	cJSON* root;
	char* save_data = NULL;
	size_t save_datalen;
	ListNode_t* cur;
	struct ClusterTable_t* table = NULL;
	int retcode = 0;
	char* reply;
	int reply_len;

	root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!root) {
		logErr(ptr_g_Log(), "Config parse extra data error");
		retcode = 1;
		goto err;
	}

	table = newClusterTable();
	if (!table) {
		logErr(ptr_g_Log(), "newClusterTable error");
		retcode = 1;
		goto err;
	}

	cjson_cluster_array = cJSON_Field(root, "clusters");
	if (!cjson_cluster_array) {
		logErr(ptr_g_Log(), "miss field cluster");
		goto err;
	}

	for (cjson_cluster = cjson_cluster_array->child; cjson_cluster; cjson_cluster = cjson_cluster->next) {
		Cluster_t* cluster;
		cJSON* name, *socktype, *ip, *port, *hashkey_array, *weight_num;

		name = cJSON_Field(cjson_cluster, "name");
		if (!name || !name->valuestring || !name->valuestring[0])
			continue;
		ip = cJSON_Field(cjson_cluster, "ip");
		if (!ip)
			continue;
		port = cJSON_Field(cjson_cluster, "port");
		if (!port)
			continue;
		socktype = cJSON_Field(cjson_cluster, "socktype");
		if (!socktype)
			continue;
		weight_num = cJSON_Field(cjson_cluster, "weight_num");
		hashkey_array = cJSON_Field(cjson_cluster, "hash_key");

		cluster = newCluster(if_string2socktype(socktype->valuestring), ip->valuestring, port->valueint);
		if (!cluster)
			goto err;
		if (hashkey_array) {
			int hashkey_arraylen = cJSON_Size(hashkey_array);
			if (hashkey_arraylen > 0) {
				int i;
				cJSON* key;
				unsigned int* ptr_key_array = reallocClusterHashKey(cluster, hashkey_arraylen);
				if (!ptr_key_array) {
					freeCluster(cluster);
					goto err;
				}
				for (i = 0, key = hashkey_array->child; key && i < hashkey_arraylen; key = key->next, ++i) {
					ptr_key_array[i] = key->valueint;
				}
			}
		}
		if (weight_num) {
			cluster->weight_num = weight_num->valueint;
		}
		if (!regCluster(table, name->valuestring, cluster)) {
			freeCluster(cluster);
			goto err;
		}
	}
	cJSON_AddNewNumber(root, "version", getClusterTableVersion() + 1);
	save_data = cJSON_Print(root);
	if (save_data)
		goto err;
	cJSON_Delete(root);
	
	save_datalen = strlen(save_data);
	if (fileWriteCoverData(ptr_g_Config()->extra_data_txt, save_data, save_datalen) != save_datalen)
		goto err;

	for (cur = getClusterList(ptr_g_ClusterTable())->head; cur; cur = cur->next) {
		Cluster_t* old_cluster = pod_container_of(cur, Cluster_t, m_listnode);
		Cluster_t* new_cluster = getClusterNode(table, old_cluster->socktype, old_cluster->ip, old_cluster->port);
		if (new_cluster) {
			Channel_t* client_channel, *server_channel;
			client_channel = old_cluster->session.channel_client;
			server_channel = old_cluster->session.channel_server;
			sessionUnbindChannel(&old_cluster->session);
			sessionChannelReplaceClient(&new_cluster->session, client_channel);
			sessionChannelReplaceServer(&new_cluster->session, server_channel);
		}
		if (getClusterSelf() == old_cluster) {
			if (new_cluster)
				setClusterSelf(new_cluster);
			else
				unregCluster(ptr_g_ClusterTable(), old_cluster);
		}
	}
	freeClusterTable(ptr_g_ClusterTable());
	set_g_ClusterTable(table);
	setClusterTableVersion(getClusterTableVersion() + 1);

	reply = "{\"retcode\":0, \"desc\":\"ok\"}";
	reply = strFormat(&reply_len, HTTP_SIMPLE_RESP_FMT, HTTP_SIMPLE_RESP_VALUE(200, reply, strlen(reply)));
	channelSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT);
	reactorCommitCmd(NULL, &ctrl->channel->_.stream_sendfincmd);
	return;
err:
	cJSON_Delete(root);
	free(save_data);
	freeClusterTable(table);
	reply = "{\"retcode\":1, \"desc\":\"failure\"}";
	reply = strFormat(&reply_len, HTTP_SIMPLE_RESP_FMT, HTTP_SIMPLE_RESP_VALUE(200, reply, strlen(reply)));
	channelSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT);
	reactorCommitCmd(NULL, &ctrl->channel->_.stream_sendfincmd);
}