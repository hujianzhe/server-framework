#include "service_comm_proc.h"

#ifdef __cplusplus
extern "C" {
#endif

int loadClusterNodeFromJsonData(struct ClusterTable_t* table, const char* data) {
	cJSON* cjson_cluster_nodes, *cjson_clsnd, *cjson_version;
	cJSON* root = cJSON_Parse(NULL, data);
	if (!root) {
		logErr(ptr_g_Log(), "Config parse extra data error");
		goto err;
	}
	if (cJSON_Field(root, "errno")) {
		logErr(ptr_g_Log(), "request errno");
		goto err;
	}
	cjson_version = cJSON_Field(root, "version");
	if (!cjson_version) {
		logErr(ptr_g_Log(), "miss field version");
		goto err;
	}
	cjson_cluster_nodes = cJSON_Field(root, "cluster_nodes");
	if (!cjson_cluster_nodes) {
		logErr(ptr_g_Log(), "miss field cluster_nodes");
		goto err;
	}
	for (cjson_clsnd = cjson_cluster_nodes->child; cjson_clsnd; cjson_clsnd = cjson_clsnd->next) {
		ClusterNode_t* clsnd;
		cJSON* name, *cjson_socktype, *ip, *port, *hashkey_array, *weight_num;
		int socktype;

		name = cJSON_Field(cjson_clsnd, "name");
		if (!name || !name->valuestring || !name->valuestring[0])
			continue;
		ip = cJSON_Field(cjson_clsnd, "ip");
		if (!ip)
			continue;
		port = cJSON_Field(cjson_clsnd, "port");
		if (!port)
			continue;
		cjson_socktype = cJSON_Field(cjson_clsnd, "socktype");
		if (!cjson_socktype)
			continue;
		socktype = if_string2socktype(cjson_socktype->valuestring);
		weight_num = cJSON_Field(cjson_clsnd, "weight_num");
		hashkey_array = cJSON_Field(cjson_clsnd, "hash_key");

		clsnd = getClusterNode(table, socktype, ip->valuestring, port->valueint);
		if (clsnd)
			continue;
		clsnd = newClusterNode(socktype, ip->valuestring, port->valueint);
		if (!clsnd)
			continue;
		if (weight_num) {
			clsnd->weight_num = weight_num->valueint;
		}
		if (hashkey_array) {
			int hashkey_arraylen = cJSON_Size(hashkey_array);
			if (hashkey_arraylen > 0) {
				int i;
				cJSON* key;
				unsigned int* ptr_key_array = reallocClusterNodeHashKey(clsnd, hashkey_arraylen);
				if (!ptr_key_array) {
					freeClusterNode(clsnd);
					continue;
				}
				for (i = 0, key = hashkey_array->child; key && i < hashkey_arraylen; key = key->next, ++i) {
					ptr_key_array[i] = key->valueint;
				}
			}
		}
		if (!regClusterNode(table, name->valuestring, clsnd)) {
			freeClusterNode(clsnd);
			continue;
		}
	}
	setClusterTableVersion(cjson_version->valueint);
	cJSON_Delete(root);
	return 1;
err:
	cJSON_Delete(root);
	return 0;
}

#ifdef __cplusplus
}
#endif