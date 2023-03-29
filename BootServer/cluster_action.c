#include "config.h"
#include "global.h"
#include "cluster_action.h"
#include <limits.h>
#include <string.h>

extern const char* ClusterNodeGroup_name(struct ClusterNodeGroup_t* grp);

#ifdef __cplusplus
extern "C" {
#endif

ClusterNode_t* flushClusterNodeFromJsonData(struct ClusterTable_t* t, const char* json_data) {
	cJSON* root;
	ClusterNode_t* clsnd = NULL;
	do {
		cJSON* cjson_ident;
		root = cJSON_FromString(json_data, 1);
		if (!root) {
			break;
		}
		cjson_ident = cJSON_GetField(root, "ident");
		if (!cjson_ident) {
			break;
		}
		clsnd = getClusterNodeById(t, cJSON_GetStringPtr(cjson_ident));
		if (!clsnd) {
			break;
		}
	} while (0);
	cJSON_Delete(root);
	return clsnd;
}

struct ClusterTable_t* loadClusterTableFromJsonData(struct ClusterTable_t* t, const char* json_data, const char** errmsg) {
	cJSON* cjson_nodes, *cjson_grps, *cjson_clsnd;
	cJSON* root;
	size_t i;

	root = cJSON_FromString(json_data, 1);
	if (!root) {
		*errmsg = "parse json data error";
		return NULL;
	}
	cjson_nodes = cJSON_GetField(root, "cluster_nodes");
	if (!cjson_nodes) {
		*errmsg = "json data miss field cluster_nodes";
		cJSON_Delete(root);
		return NULL;
	}
	cjson_grps = cJSON_GetField(root, "cluster_node_groups");
	if (!cjson_grps) {
		*errmsg = "json data miss field cluster_node_groups";
		cJSON_Delete(root);
		return NULL;
	}

	*errmsg = NULL;
	for (cjson_clsnd = cjson_nodes->child; cjson_clsnd; cjson_clsnd = cjson_clsnd->next) {
		ClusterNode_t* clsnd;
		cJSON *cjson_ident, * cjson_socktype, *cjson_ip, *port;
		int socktype;
		const char* ident, *ip;

		cjson_ident = cJSON_GetField(cjson_clsnd, "ident");
		if (!cjson_ident) {
			continue;
		}
		ident = cJSON_GetStringPtr(cjson_ident);
		if (!ident || 0 == *ident) {
			continue;
		}
		cjson_ip = cJSON_GetField(cjson_clsnd, "ip");
		if (!cjson_ip) {
			continue;
		}
		ip = cJSON_GetStringPtr(cjson_ip);
		if (!ip || 0 == *ip) {
			continue;
		}
		port = cJSON_GetField(cjson_clsnd, "port");
		if (!port) {
			continue;
		}
		cjson_socktype = cJSON_GetField(cjson_clsnd, "socktype");
		if (!cjson_socktype) {
			continue;
		}
		socktype = if_string2socktype(cJSON_GetStringPtr(cjson_socktype));
		if (0 == socktype) {
			continue;
		}

		clsnd = getClusterNodeById(t, ident);
		if (!clsnd) {
			clsnd = newClusterNode(ident, socktype, ip, cJSON_GetInteger(port));
			if (!clsnd) {
				goto err;
			}
			if (!clusterAddNode(t, clsnd)) {
				freeClusterNode(clsnd);
				goto err;
			}
		}
	}
	for (cjson_clsnd = cjson_grps->child; cjson_clsnd; cjson_clsnd = cjson_clsnd->next) {
		ClusterNode_t* clsnd;
		struct ClusterNodeGroup_t* grp;
		cJSON *cjson_name, *cjson_ident, *cjson_hashkey_array, *cjson_weight_num;
		const char* name, *ident;
		int ret_ok, weight_num;

		cjson_name = cJSON_GetField(cjson_clsnd, "name");
		name = cJSON_GetStringPtr(cjson_name);
		if (!name || 0 == *name) {
			continue;
		}
		cjson_ident = cJSON_GetField(cjson_clsnd, "ident");
		if (!cjson_ident) {
			continue;
		}
		ident = cJSON_GetStringPtr(cjson_ident);
		if (!ident || 0 == *ident) {
			continue;		
		}

		clsnd = getClusterNodeById(t, ident);
		if (!clsnd) {
			continue;
		}
		grp = getClusterNodeGroup(t, name);
		if (!grp) {
			grp = newClusterNodeGroup(name);
			if (!grp) {
				goto err;
			}
			replaceClusterNodeGroup(t, grp);
		}
		if (!regClusterNodeToGroup(grp, clsnd)) {
			goto err;
		}
		cjson_hashkey_array = cJSON_GetField(cjson_clsnd, "hash_key");
		if (cjson_hashkey_array) {
			cJSON* key;
			for (key = cjson_hashkey_array->child; key; key = key->next) {
				unsigned int hashkey;
				if (cJSON_GetDouble(key) < 1.0) {
					hashkey = cJSON_GetDouble(key) * UINT_MAX;
				}
				else {
					hashkey = cJSON_GetInteger(key);
				}
				if (!regClusterNodeToGroupByHashKey(grp, hashkey, clsnd)) {
					goto err;
				}
			}
		}
		cjson_weight_num = cJSON_GetField(cjson_clsnd, "weight_num");
		if (cjson_weight_num && (weight_num = cJSON_GetInteger(cjson_weight_num)) > 0) {
			if (!regClusterNodeToGroupByWeight(grp, weight_num, clsnd)) {
				goto err;
			}
		}
	}
	cJSON_Delete(root);
	return t;
err:
	cJSON_Delete(root);
	return NULL;
}

#ifdef __cplusplus
}
#endif
