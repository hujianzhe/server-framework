#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "../InnerProcHandle/inner_proc_cmd.h"
#include "service_center_handler.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

static int loadClusterNode(const char* data) {
	cJSON* cluster_grp_array, *cluster_grp;
	cJSON* root = cJSON_Parse(NULL, data);
	if (!root) {
		logErr(ptr_g_Log(), "Config parse extra data error");
		return 0;
	}
	cluster_grp_array = cJSON_Field(root, "cluster_grouop");
	if (!cluster_grp_array) {
		logErr(ptr_g_Log(), "miss field cluster");
		return 0;
	}
	for (cluster_grp = cluster_grp_array->child; cluster_grp; cluster_grp = cluster_grp->next) {
		cJSON* name, *cluster_array, *node;
		name = cJSON_Field(cluster_grp, "name");
		if (!name)
			continue;
		cluster_array = cJSON_Field(cluster_grp, "cluster");
		if (!cluster_array || cJSON_Size(cluster_array) == 0)
			continue;
		for (node = cluster_array->child; node; node = node->next) {
			Cluster_t* cluster;
			cJSON* socktype, *ip, *port, *key_array;
			socktype = cJSON_Field(node, "socktype");
			ip = cJSON_Field(node, "ip");
			if (!ip)
				continue;
			port = cJSON_Field(node, "port");
			if (!port)
				continue;
			cluster = newCluster();
			if (!cluster)
				continue;
			key_array = cJSON_Field(node, "key");
			if (key_array) {
				int key_arraylen = cJSON_Size(key_array);
				if (key_arraylen > 0) {
					int i;
					cJSON* key;
					unsigned int* ptr_key_array = newClusterKeyArray(cluster, key_arraylen);
					if (!ptr_key_array) {
						freeCluster(cluster);
						continue;
					}
					for (i = 0, key = key_array->child; key && i < key_arraylen; key = key->next, ++i) {
						ptr_key_array[i] = key->valueint;
					}
				}
			}
			if (!socktype) {
				cluster->socktype = SOCK_STREAM;
			}
			else {
				cluster->socktype = if_string2socktype(socktype->valuestring);
			}
			strcpy(cluster->ip, ip->valuestring);
			cluster->port = port->valueint;
			if (!regCluster(name->valuestring, cluster)) {
				freeCluster(cluster);
				continue;
			}
		}
	}
	cJSON_Delete(root);
	return 1;
}

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(int argc, char** argv) {
	if (!loadClusterNode(ptr_g_Config()->extra_data_txt))
		return 0;

	regStringDispatch("/get_cluster_list", reqClusterList_http);
	regNumberDispatch(CMD_REQ_CLUSTER_LIST, reqClusterList);
	// regNumberDispatch(CMD_REQ_CLUSTER_TELL_SELF, reqClusterTellSelf);

	return 1;
}

#ifdef __cplusplus
}
#endif