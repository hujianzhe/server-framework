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
		cJSON* cjson_id;
		root = cJSON_FromString(json_data, 0);
		if (!root) {
			break;
		}
		cjson_id = cJSON_GetField(root, "id");
		if (!cjson_id) {
			break;
		}
		clsnd = getClusterNodeById(t, cJSON_GetInteger(cjson_id));
		if (!clsnd) {
			break;
		}
	} while (0);
	cJSON_Delete(root);
	return clsnd;
}

static ClusterNode_t* find_clsnd_(struct ClusterTable_t* t, List_t* new_clsnds, int id) {
	ListNode_t* lcur;
	ClusterNode_t* clsnd = getClusterNodeById(t, id);
	if (clsnd) {
		return clsnd;
	}
	for (lcur = new_clsnds->head; lcur; lcur = lcur->next) {
		clsnd = pod_container_of(lcur, ClusterNode_t, m_listnode);
		if (id == clsnd->id) {
			return clsnd;
		}
	}
	return NULL;
}

struct ClusterTable_t* loadClusterTableFromJsonData(struct ClusterTable_t* t, const char* json_data, const char** errmsg) {
	DynArr_t(struct ClusterNodeGroup_t*) new_grps = { 0 };
	List_t new_clsnds;
	ListNode_t* lcur, * lnext;
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
	listInit(&new_clsnds);
	for (cjson_clsnd = cjson_nodes->child; cjson_clsnd; cjson_clsnd = cjson_clsnd->next) {
		ClusterNode_t* clsnd;
		cJSON *cjson_id, * cjson_socktype, *ip, *port;
		int socktype;
		int id;

		cjson_id = cJSON_GetField(cjson_clsnd, "id");
		if (!cjson_id) {
			continue;
		}
		ip = cJSON_GetField(cjson_clsnd, "ip");
		if (!ip) {
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
		id = cJSON_GetInteger(cjson_id);

		if (find_clsnd_(t, &new_clsnds, id)) {
			continue;
		}
		clsnd = newClusterNode(id, socktype, cJSON_GetStringPtr(ip), cJSON_GetInteger(port));
		if (!clsnd) {
			goto err;
		}
		listPushNodeBack(&new_clsnds, &clsnd->m_listnode);
	}
	for (cjson_clsnd = cjson_grps->child; cjson_clsnd; cjson_clsnd = cjson_clsnd->next) {
		ClusterNode_t* clsnd;
		struct ClusterNodeGroup_t* grp;
		cJSON *cjson_name, *cjson_id, *cjson_hashkey_array, *cjson_weight_num;
		const char* name;
		int ret_ok, id, weight_num;

		cjson_name = cJSON_GetField(cjson_clsnd, "name");
		name = cJSON_GetStringPtr(cjson_name);
		if (!name || 0 == *name) {
			continue;
		}
		cjson_id = cJSON_GetField(cjson_clsnd, "id");
		if (!cjson_id) {
			continue;
		}
		ret_ok = 1;
		grp = NULL;
		for (i = 0; i < new_grps.len; ++i) {
			if (!strcmp(new_grps.buf[i]->name, name)) {
				grp = new_grps.buf[i];
				break;
			}
		}
		if (!grp) {
			grp = newClusterNodeGroup(name);
			if (!grp) {
				goto err;
			}
			dynarrInsert(&new_grps, new_grps.len, grp, ret_ok);
			if (!ret_ok) {
				goto err;
			}
		}
		id = cJSON_GetInteger(cjson_id);
		clsnd = find_clsnd_(t, &new_clsnds, id);
		if (!clsnd) {
			continue;
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
	clearClusterNodeGroup(t);
	for (i = 0; i < new_grps.len; ++i) {
		replaceClusterNodeGroup(t, new_grps.buf[i]);
	}
	cJSON_Delete(root);
	dynarrFreeMemory(&new_grps);
	return t;
err:
	cJSON_Delete(root);
	for (lcur = new_clsnds.head; lcur; lcur = lnext) {
		ClusterNode_t* clsnd = pod_container_of(lcur, ClusterNode_t, m_listnode);
		lnext = lcur->next;
		freeClusterNode(clsnd);
	}
	for (i = 0; i < new_grps.len; ++i) {
		freeClusterNodeGroup(new_grps.buf[i]);
	}
	dynarrFreeMemory(&new_grps);
	return NULL;
}

#ifdef __cplusplus
}
#endif
