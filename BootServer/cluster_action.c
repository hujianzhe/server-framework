#include "global.h"
#include "cluster_action.h"

#ifdef __cplusplus
extern "C" {
#endif

ClusterNode_t* flushClusterNodeFromJsonData(struct ClusterTable_t* t, const char* json_data) {
	cJSON* root;
	ClusterNode_t* clsnd = NULL;
	do {
		cJSON *cjson_socktype, *cjson_ip, *cjson_port, *cjson_conn_num, *cjson_weight_num;
		int socktype;

		root = cJSON_Parse(NULL, json_data);
		if (!root) {
			break;
		}
		cjson_socktype = cJSON_Field(root, "socktype");
		if (!cjson_socktype) {
			break;
		}
		socktype = if_string2socktype(cjson_socktype->valuestring);
		cjson_ip = cJSON_Field(root, "ip");
		if (!cjson_ip) {
			break;
		}
		cjson_port = cJSON_Field(root, "port");
		if (!cjson_port) {
			break;
		}
		cjson_conn_num = cJSON_Field(root, "connection_num");
		cjson_weight_num = cJSON_Field(root, "weight_num");

		clsnd = getClusterNode(t, socktype, cjson_ip->valuestring, cjson_port->valueint);
		if (!clsnd) {
			break;
		}
		if (cjson_conn_num && cjson_conn_num->valueint > 0)
			clsnd->connection_num = cjson_conn_num->valueint;
		if (cjson_weight_num && cjson_weight_num->valueint >= 0)
			clsnd->weight_num = cjson_weight_num->valueint;
	} while (0);
	cJSON_Delete(root);
	return clsnd;
}

const char* loadClusterTableFromJsonData(struct ClusterTable_t* t, const char* json_data) {
	const char* errmsg;
	cJSON* root = cJSON_Parse(NULL, json_data);
	if (!root) {
		errmsg = "parse json data error";
		return 0;
	}
	errmsg = NULL;
	do {
		cJSON* cjson_cluster_nodes, *cjson_clsnd, *cjson_version;

		cjson_version = cJSON_Field(root, "version");
		if (!cjson_version) {
			errmsg = "json data miss field version";
			break;
		}
		cjson_cluster_nodes = cJSON_Field(root, "cluster_nodes");
		if (!cjson_cluster_nodes) {
			errmsg = "json data miss field cluster_nodes";
			break;
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

			clsnd = getClusterNode(t, socktype, ip->valuestring, port->valueint);
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
						break;
					}
					for (i = 0, key = hashkey_array->child; key && i < hashkey_arraylen; key = key->next, ++i) {
						if (key->valuedouble < 1.0) {
							ptr_key_array[i] = key->valuedouble * UINT_MAX;
						}
						else {
							ptr_key_array[i] = key->valueint;
						}
					}
				}
			}
			if (!regClusterNode(t, name->valuestring, clsnd)) {
				freeClusterNode(clsnd);
				break;
			}
		}
		if (cjson_clsnd) {
			errmsg = "reg cluster node error";
			break;
		}
		g_ClusterTableVersion = cjson_version->valueint;
	} while (0);
	cJSON_Delete(root);
	return errmsg;
}

#ifdef __cplusplus
}
#endif