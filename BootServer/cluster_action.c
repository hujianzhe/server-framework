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
		root = cJSON_Parse(NULL, json_data);
		if (!root) {
			break;
		}
		cjson_id = cJSON_Field(root, "id");
		if (!cjson_id) {
			break;
		}
		clsnd = getClusterNodeById(t, cjson_id->valueint);
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

	root = cJSON_Parse(NULL, json_data);
	if (!root) {
		*errmsg = "parse json data error";
		return NULL;
	}
	cjson_nodes = cJSON_Field(root, "cluster_nodes");
	if (!cjson_nodes) {
		*errmsg = "json data miss field cluster_nodes";
		cJSON_Delete(root);
		return NULL;
	}
	cjson_grps = cJSON_Field(root, "cluster_node_groups");
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

		id = cJSON_Field(cjson_clsnd, "id");
		if (!id) {
			continue;
		}
		ip = cJSON_Field(cjson_clsnd, "ip");
		if (!ip) {
			continue;
		}
		port = cJSON_Field(cjson_clsnd, "port");
		if (!port) {
			continue;
		}
		cjson_socktype = cJSON_Field(cjson_clsnd, "socktype");
		if (!cjson_socktype) {
			continue;
		}
		socktype = if_string2socktype(cjson_socktype->valuestring);
		if (0 == socktype) {
			continue;
		}

		clsnd = getClusterNodeById(t, id->valueint);
		if (clsnd) {
			clsnd->status = CLSND_STATUS_NORMAL;
		}
		else {
			ListNode_t* lcur;
			for (lcur = new_clsnds.head; lcur; lcur = lcur->next) {
				ClusterNode_t* obj = pod_container_of(lcur, ClusterNode_t, m_listnode);
				if (id->valueint == obj->id) {
					clsnd = obj;
					break;
				}
			}
			if (!clsnd) {
				clsnd = newClusterNode(id->valueint, socktype, ip->valuestring, port->valueint);
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
		cJSON *name, *id, *hashkey_array, *weight_num;
		int ret_ok;
		size_t i;

		name = cJSON_Field(cjson_clsnd, "name");
		if (!name || !name->valuestring || !name->valuestring[0]) {
			continue;
		}
		id = cJSON_Field(cjson_clsnd, "id");
		if (!id) {
			continue;
		}
		weight_num = cJSON_Field(cjson_clsnd, "weight_num");
		hashkey_array = cJSON_Field(cjson_clsnd, "hash_key");

		ret_ok = 1;
		grp = NULL;
		for (i = 0; i < new_grps.len; ++i) {
			if (!strcmp(new_grps.buf[i]->name, name->valuestring)) {
				grp = new_grps.buf[i];
				break;
			}
		}
		if (!grp) {
			grp = newClusterNodeGroup(name->valuestring);
			if (!grp) {
				break;
			}
			dynarrInsert(&new_grps, new_grps.len, grp, ret_ok);
			if (!ret_ok) {
				break;
			}
		}

		clsnd = getClusterNodeById(t, id->valueint);
		if (!clsnd) {
			for (lcur = new_clsnds.head; lcur; lcur = lcur->next) {
				ClusterNode_t* obj = pod_container_of(lcur, ClusterNode_t, m_listnode);
				if (id->valueint == obj->id) {
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
			int hashkey_arrlen;
			if (!hashkey_array) {
				break;
			}
			hashkey_arrlen = cJSON_Size(hashkey_array);
			if (hashkey_arrlen <= 0) {
				break;
			}
			for (key = hashkey_array->child; key; key = key->next) {
				unsigned int hashkey;
				if (key->valuedouble < 1.0) {
					hashkey = key->valuedouble * UINT_MAX;
				}
				else {
					hashkey = key->valueint;
				}
				if (!consistenthashReg(&grp->consistent_hash, hashkey, clsnd)) {
					ret_ok = 0;
					break;
				}
			}
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
