#include "inner_proc_cluster.h"
#include "inner_proc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

void distributeClusterList(TaskThread_t* thrd, UserMsg_t* ctrl) {
	cJSON* cjson_cluster_array, *cjson_cluster;
	cJSON* root;
	struct ClusterTable_t* table = NULL;
	ListNode_t* cur;

	root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!root) {
		logErr(ptr_g_Log(), "Config parse extra data error");
		goto err;
	}

	table = newClusterTable();
	if (!table) {
		logErr(ptr_g_Log(), "newClusterTable error");
		goto err;
	}

	cjson_cluster_array = cJSON_Field(root, "clusters");
	if (!cjson_cluster_array) {
		logErr(ptr_g_Log(), "miss field cluster");
		goto err;
	}

	for (cjson_cluster = cjson_cluster_array->child; cjson_cluster; cjson_cluster = cjson_cluster->next) {
		Cluster_t* cluster;
		cJSON* name, *socktype, *ip, *port, *hashkey_array;

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
		if (!regCluster(table, name->valuestring, cluster)) {
			freeCluster(cluster);
			goto err;
		}
	}
	cJSON_Delete(root);

	for (cur = getClusterList(ptr_g_ClusterTable())->head; cur; cur = cur->next) {
		Cluster_t* old_cluster = pod_container_of(cur, Cluster_t, m_listnode);
		Cluster_t* new_cluster = getCluster(table, old_cluster->name, old_cluster->ip, old_cluster->port);
		if (new_cluster) {
			Channel_t* channel;
			channel = old_cluster->session.channel_client;
			if (channel) {
				sessionUnbindChannel(&old_cluster->session, channel);
				sessionChannelReplaceClient(&new_cluster->session, channel);
			}
			channel = old_cluster->session.channel_server;
			if (channel) {
				sessionUnbindChannel(&old_cluster->session, channel);
				sessionChannelReplaceServer(&new_cluster->session, channel);
			}
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

	return;
err:
	cJSON_Delete(root);
	freeClusterTable(table);
}

#ifdef __cplusplus
}
#endif