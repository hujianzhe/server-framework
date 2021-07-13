#include "config.h"
#include "global.h"
#include "cluster_action.h"
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

ClusterNode_t* flushClusterNodeFromJsonData(struct ClusterTable_t* t, const char* json_data) {
	cJSON* root;
	ClusterNode_t* clsnd = NULL;
	do {
		cJSON *cjson_id, *cjson_socktype, *cjson_ip, *cjson_port, *cjson_conn_num, *cjson_weight_num;
		int socktype;

		root = cJSON_Parse(NULL, json_data);
		if (!root) {
			break;
		}
		cjson_id = cJSON_Field(root, "id");
		if (!cjson_id) {
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

		clsnd = getClusterNodeById(t, cjson_id->valueint);
		if (!clsnd) {
			break;
		}
		if (clsnd->socktype != socktype ||
			clsnd->port != cjson_port->valueint ||
			strcmp(clsnd->ip, cjson_ip->valuestring))
		{
			clsnd = NULL;
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

struct ClusterTable_t* loadClusterTableFromJsonData(struct ClusterTable_t* t, const char* json_data, const char** errmsg) {
	ListNode_t* listnode_cur;
	cJSON* cjson_cluster_nodes, *cjson_clsnd;
	cJSON* root = cJSON_Parse(NULL, json_data);
	if (!root) {
		*errmsg = "parse json data error";
		return NULL;
	}
	cjson_cluster_nodes = cJSON_Field(root, "cluster_nodes");
	if (!cjson_cluster_nodes) {
		*errmsg = "json data miss field cluster_nodes";
		cJSON_Delete(root);
		return NULL;
	}
	*errmsg = NULL;
	for (cjson_clsnd = cjson_cluster_nodes->child; cjson_clsnd; cjson_clsnd = cjson_clsnd->next) {
		ClusterNode_t* clsnd;
		cJSON *name, *id, *cjson_socktype, *ip, *port, *hashkey_array, *weight_num;
		int socktype;

		name = cJSON_Field(cjson_clsnd, "name");
		if (!name || !name->valuestring || !name->valuestring[0])
			continue;
		id = cJSON_Field(cjson_clsnd, "id");
		if (!id)
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
		if (0 == socktype)
			continue;
		weight_num = cJSON_Field(cjson_clsnd, "weight_num");
		hashkey_array = cJSON_Field(cjson_clsnd, "hash_key");

		clsnd = getClusterNodeById(t, id->valueint);
		if (clsnd) {
			if (weight_num) {
				clsnd->weight_num = weight_num->valueint;
			}
			clsnd->status = CLSND_STATUS_NORMAL;
		}
		else {
			clsnd = newClusterNode(id->valueint, socktype, ip->valuestring, port->valueint);
			if (!clsnd) {
				break;
			}
			if (weight_num) {
				clsnd->weight_num = weight_num->valueint;
			}
			do {
				int i;
				cJSON* key;
				unsigned int* ptr_key_array;
				int hashkey_arraylen;
				if (!hashkey_array) {
					break;
				}
				hashkey_arraylen = cJSON_Size(hashkey_array);
				if (hashkey_arraylen <= 0) {
					break;
				}
				ptr_key_array = reallocClusterNodeHashKey(clsnd, hashkey_arraylen);
				if (!ptr_key_array) {
					freeClusterNode(clsnd);
					clsnd = NULL;
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
			} while (0);
			if (!clsnd) {
				break;
			}
			if (!regClusterNode(t, name->valuestring, clsnd)) {
				freeClusterNode(clsnd);
				break;
			}
		}
	}
	if (cjson_clsnd) {
		cJSON_Delete(root);
		return NULL;
	}
	for (listnode_cur = getClusterNodeList(t)->head; listnode_cur; listnode_cur = listnode_cur->next) {
		ClusterNode_t* clsnd = pod_container_of(listnode_cur, ClusterNode_t, m_listnode);
		for (cjson_clsnd = cjson_cluster_nodes->child; cjson_clsnd; cjson_clsnd = cjson_clsnd->next) {
			cJSON* id = cJSON_Field(cjson_clsnd, "id");
			if (!id) {
				continue;
			}
			if (id->valueint == clsnd->id) {
				break;
			}
		}
		if (cjson_clsnd) {
			continue;
		}
		clsnd->status = CLSND_STATUS_INACTIVE;
	}
	cJSON_Delete(root);
	return t;
}

#ifdef __cplusplus
}
#endif
