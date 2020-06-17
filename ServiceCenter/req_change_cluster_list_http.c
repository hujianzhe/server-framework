#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "service_center_handler.h"

void reqChangeClusterNode_http(TaskThread_t* thrd, UserMsg_t* ctrl) {
	cJSON* cjson_cluster_nodes, *cjson_clsnd;
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

	cjson_cluster_nodes = cJSON_Field(root, "cluster_nodes");
	if (!cjson_cluster_nodes) {
		logErr(ptr_g_Log(), "miss field cluster");
		goto err;
	}

	for (cjson_clsnd = cjson_cluster_nodes->child; cjson_clsnd; cjson_clsnd = cjson_clsnd->next) {
		ClusterNode_t* clsnd;
		cJSON* name, *socktype, *ip, *port, *hashkey_array, *weight_num;

		name = cJSON_Field(cjson_clsnd, "name");
		if (!name || !name->valuestring || !name->valuestring[0])
			continue;
		ip = cJSON_Field(cjson_clsnd, "ip");
		if (!ip)
			continue;
		port = cJSON_Field(cjson_clsnd, "port");
		if (!port)
			continue;
		socktype = cJSON_Field(cjson_clsnd, "socktype");
		if (!socktype)
			continue;
		weight_num = cJSON_Field(cjson_clsnd, "weight_num");
		hashkey_array = cJSON_Field(cjson_clsnd, "hash_key");

		clsnd = newClusterNode(if_string2socktype(socktype->valuestring), ip->valuestring, port->valueint);
		if (!clsnd)
			goto err;
		if (hashkey_array) {
			int hashkey_arraylen = cJSON_Size(hashkey_array);
			if (hashkey_arraylen > 0) {
				int i;
				cJSON* key;
				unsigned int* ptr_key_array = reallocClusterNodeHashKey(clsnd, hashkey_arraylen);
				if (!ptr_key_array) {
					freeClusterNode(clsnd);
					goto err;
				}
				for (i = 0, key = hashkey_array->child; key && i < hashkey_arraylen; key = key->next, ++i) {
					ptr_key_array[i] = key->valueint;
				}
			}
		}
		if (weight_num) {
			clsnd->weight_num = weight_num->valueint;
		}
		if (!regClusterNode(table, name->valuestring, clsnd)) {
			freeClusterNode(clsnd);
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

	for (cur = getClusterNodeList(ptr_g_ClusterTable())->head; cur; cur = cur->next) {
		ClusterNode_t* old_clsnd = pod_container_of(cur, ClusterNode_t, m_listnode);
		ClusterNode_t* new_clsnd = getClusterNode(table, old_clsnd->socktype, old_clsnd->ip, old_clsnd->port);
		if (new_clsnd) {
			Channel_t* client_channel, *server_channel;
			client_channel = old_clsnd->session.channel_client;
			server_channel = old_clsnd->session.channel_server;
			sessionUnbindChannel(&old_clsnd->session);
			sessionChannelReplaceClient(&new_clsnd->session, client_channel);
			sessionChannelReplaceServer(&new_clsnd->session, server_channel);
		}
		if (selfClusterNode() == old_clsnd) {
			if (new_clsnd)
				setSelfClusterNode(new_clsnd);
			else
				unregClusterNode(ptr_g_ClusterTable(), old_clsnd);
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