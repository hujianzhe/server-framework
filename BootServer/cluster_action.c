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
		cJSON *id, * cjson_socktype, *ip, *port;
		int socktype;

		id = cJSON_GetField(cjson_clsnd, "id");
		if (!id) {
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

		clsnd = getClusterNodeById(t, cJSON_GetInteger(id));
		if (clsnd) {
			clsnd->status = CLSND_STATUS_NORMAL;
		}
		else {
			ListNode_t* lcur;
			for (lcur = new_clsnds.head; lcur; lcur = lcur->next) {
				ClusterNode_t* obj = pod_container_of(lcur, ClusterNode_t, m_listnode);
				if (cJSON_GetInteger(id) == obj->id) {
					clsnd = obj;
					break;
				}
			}
			if (!clsnd) {
				clsnd = newClusterNode(cJSON_GetInteger(id), socktype, cJSON_GetStringPtr(ip), cJSON_GetInteger(port));
				if (!clsnd) {
					break;
				}
				listPushNodeBack(&new_clsnds, &clsnd->m_listnode);
			}
		}
	}
	if (cjson_clsnd) {
		goto err;
	}
	for (cjson_clsnd = cjson_grps->child; cjson_clsnd; cjson_clsnd = cjson_clsnd->next) {
		ClusterNode_t* clsnd;
		struct ClusterNodeGroup_t* grp;
		cJSON *name, *id;
		int ret_ok;
		size_t i;

		name = cJSON_GetField(cjson_clsnd, "name");
		if (!name || !cJSON_GetStringPtr(name) || !cJSON_GetStringLength(name)) {
			continue;
		}
		id = cJSON_GetField(cjson_clsnd, "id");
		if (!id) {
			continue;
		}
		ret_ok = 1;
		grp = NULL;
		for (i = 0; i < new_grps.len; ++i) {
			if (!strcmp(new_grps.buf[i]->name, cJSON_GetStringPtr(name))) {
				grp = new_grps.buf[i];
				break;
			}
		}
		if (!grp) {
			grp = newClusterNodeGroup(cJSON_GetStringPtr(name));
			if (!grp) {
				break;
			}
			dynarrInsert(&new_grps, new_grps.len, grp, ret_ok);
			if (!ret_ok) {
				break;
			}
		}

		clsnd = getClusterNodeById(t, cJSON_GetInteger(id));
		if (!clsnd) {
			for (lcur = new_clsnds.head; lcur; lcur = lcur->next) {
				ClusterNode_t* obj = pod_container_of(lcur, ClusterNode_t, m_listnode);
				if (cJSON_GetInteger(id) == obj->id) {
					clsnd = obj;
					break;
				}
			}
			if (!clsnd) {
				continue;
			}
		}
		if (!regClusterNodeToGroup(grp, clsnd)) {
			break;
		}
		do {
			cJSON* key;
			cJSON* hashkey_array = cJSON_GetField(cjson_clsnd, "hash_key");
			if (!hashkey_array) {
				break;
			}
			if (cJSON_ChildNum(hashkey_array) <= 0) {
				break;
			}
			for (key = hashkey_array->child; key; key = key->next) {
				struct {
					RBTreeNode_t _;
					ClusterNode_t* clsnd;
				} *data;
				RBTreeNode_t* exist_node;
				unsigned int hashkey;
				if (cJSON_GetDouble(key) < 1.0) {
					hashkey = cJSON_GetDouble(key) * UINT_MAX;
				}
				else {
					hashkey = cJSON_GetInteger(key);
				}
				*(void**)&data = malloc(sizeof(*data));
				if (!data) {
					ret_ok = 0;
					break;
				}
				data->_.key.u32 = hashkey;
				data->clsnd = clsnd;
				exist_node = rbtreeInsertNode(&grp->consistent_hash_ring, &data->_);
				if (exist_node != &data->_) {
					free(exist_node);
					rbtreeReplaceNode(exist_node, &data->_);
				}
			}
		} while (0);
		do {
			struct {
				RBTreeNode_t _;
				ClusterNode_t* clsnd;
			} *data;
			cJSON* weight_num = cJSON_GetField(cjson_clsnd, "weight_num");
			if (!weight_num || cJSON_GetInteger(weight_num) <= 0) {
				break;
			}
			*(void**)&data = malloc(sizeof(*data));
			if (!data) {
				ret_ok = 0;
				break;
			}
			grp->total_weight += cJSON_GetInteger(weight_num);
			data->_.key.u32 = grp->total_weight;
			data->clsnd = clsnd;
			rbtreeInsertNode(&grp->weight_num_ring, &data->_);
		} while (0);
		if (!ret_ok) {
			break;
		}
	}
	if (cjson_clsnd) {
		goto err;
	}
	replaceClusterNodeGroup(t, new_grps.buf, new_grps.len);
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
