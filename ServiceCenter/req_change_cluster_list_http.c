#include "../BootServer/global.h"
#include "service_center_handler.h"

void reqChangeClusterNode_http(UserMsg_t* ctrl) {
	cJSON* root, *cjson_modify, *cjson_delete;
	int retcode = 0;

	root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!root) {
		logErr(ptr_g_Log(), "Config parse extra data error");
		retcode = 1;
		goto err;
	}
	cjson_modify = cJSON_Field(root, "modify");
	cjson_delete = cJSON_Field(root, "delete");

	if (cjson_modify) {
		cJSON* cjson_cluster;
		for (cjson_cluster = cjson_modify->child; cjson_cluster; cjson_cluster = cjson_cluster->next) {
			cJSON* name, *ip, *port;
			Cluster_t* cluster;

			name = cJSON_Field(cjson_cluster, "name");
			if (!name)
				continue;
			ip = cJSON_Field(cjson_cluster, "ip");
			if (!ip)
				continue;
			port = cJSON_Field(cjson_cluster, "port");
			if (!port)
				continue;

			cluster = getCluster(name->valuestring, ip->valuestring, port->valueint);
			if (cluster) {
				int hashkey_arraylen, i;
				unsigned int* ptr_hashkey;
				cJSON* cjson_hashkey_array, *cjson_hashkey;
				cjson_hashkey_array = cJSON_Field(cjson_cluster, "hash_key");
				if (!cjson_hashkey_array) {
					// TODO unreg consistent hash
					continue;
				}
				hashkey_arraylen = cJSON_Size(cjson_hashkey_array);
				if (hashkey_arraylen <= 0) {
					// TODO unreg consistent hash
					continue;
				}
				ptr_hashkey = reallocClusterHashKey(cluster, hashkey_arraylen);
				if (!ptr_hashkey) {
					continue;
				}
				for (i = 0, cjson_hashkey = cjson_hashkey_array->child;
					cjson_hashkey && i < hashkey_arraylen;
					++i, cjson_hashkey = cjson_hashkey->next)
				{
					ptr_hashkey[i] = cjson_hashkey->valueint;
				}
				// TODO unreg and reg consistent hash
			}
			else {
				cJSON* cjson_socktype;
				cluster = newCluster();
				if (!cluster) {
					continue;
				}
				strcpy(cluster->ip, ip->valuestring);
				cluster->port = port->valueint;
				cjson_socktype = cJSON_Field(cjson_cluster, "socktype");
				if (cjson_socktype)
					cluster->socktype = if_string2socktype(cjson_socktype->valuestring);
				else
					cluster->socktype = SOCK_STREAM;
				if (!regCluster(name->valuestring, cluster))
					continue;
			}
		}
	}

	if (cjson_delete) {
		cJSON* cjson_cluster;
		for (cjson_cluster = cjson_delete->child; cjson_cluster; cjson_cluster = cjson_cluster->next) {
			cJSON* name, *ip, *port;
			Cluster_t* cluster;

			name = cJSON_Field(cjson_cluster, "name");
			if (!name)
				continue;
			ip = cJSON_Field(cjson_cluster, "ip");
			if (!ip)
				continue;
			port = cJSON_Field(cjson_cluster, "port");
			if (!port)
				continue;
			cluster = getCluster(name->valuestring, ip->valuestring, port->valueint);
			if (!cluster)
				continue;
			unregCluster(cluster);
			freeCluster(cluster);
		}
	}

	return;
err:
	cJSON_Delete(root);
}