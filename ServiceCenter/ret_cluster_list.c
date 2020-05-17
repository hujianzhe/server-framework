#include "../BootServer/global.h"
#include "service_center_handler.h"
#include <stdio.h>

void retClusterList(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;
	cJSON* cjson_session_id, *cjson_cluster_array, *cjson_cluster;

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		fputs("cJSON_Parse", stderr);
		return;
	}
	printf("recv: %s\n", (char*)(ctrl->data));	

	if (!cJSON_Field(cjson_req_root, "errno")) {
		goto err;
	}

	cjson_session_id = cJSON_Field(cjson_req_root, "session_id");
	if (!cjson_session_id) {
		goto err;
	}
	cjson_cluster_array = cJSON_Field(cjson_req_root, "cluster");
	if (!cjson_cluster_array) {
		goto err;
	}
	for (cjson_cluster = cjson_cluster_array->child; cjson_cluster; cjson_cluster = cjson_cluster->next) {
		Cluster_t* cluster;
		cJSON* name, *socktype, *ip, *port;
		name = cJSON_Field(cjson_cluster, "name");
		if (!name)
			continue;
		socktype = cJSON_Field(cjson_cluster, "socktype");
		if (!socktype)
			continue;
		ip = cJSON_Field(cjson_cluster, "ip");
		if (!ip)
			continue;
		port = cJSON_Field(cjson_cluster, "port");
		if (!port)
			continue;
		if (!strcmp(ptr_g_ClusterSelf()->name, name->valuestring) &&
			!strcmp(ptr_g_ClusterSelf()->ip, ip->valuestring) &&
			ptr_g_ClusterSelf()->port == port->valueint)
		{
			ptr_g_ClusterSelf()->socktype = if_string2socktype(socktype->valuestring);
			continue;
		}
		cluster = newCluster();
		if (!cluster) {
			break;
		}
		cluster->socktype = if_string2socktype(socktype->valuestring);
		strcpy(cluster->ip, ip->valuestring);
		cluster->port = port->valueint;
		if (!regCluster(name->valuestring, cluster)) {
			freeCluster(cluster);
			break;
		}
	}
	if (cjson_cluster) {
		goto err;
	}
	channelSessionId(ctrl->channel) = cjson_session_id->valueint;
	cJSON_Delete(cjson_req_root);
	return;
err:
	cJSON_Delete(cjson_req_root);
}