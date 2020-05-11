#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "service_center_handler.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

static int loadClusterNode(const char* path) {
	cJSON* cluster_grp_array, *cluster_grp;
	cJSON* root = cJSON_ParseFromFile(NULL, path);
	if (!root) {
		puts("ServiceConfig.txt parse error");
		return 0;
	}
	cluster_grp_array = cJSON_Field(root, "cluster_grouop");
	if (!cluster_grp_array) {
		puts("miss field cluster");
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
			cJSON* socktype, *ip, *port;
			socktype = cJSON_Field(node, "socktype");
			if (!socktype)
				continue;
			ip = cJSON_Field(node, "ip");
			if (!ip)
				continue;
			port = cJSON_Field(node, "port");
			if (!port)
				continue;
			cluster = newCluster();
			if (!cluster)
				continue;
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
	if (!loadClusterNode("../ServiceCenter/ServiceConfig.txt"))
		return 0;

	regStringDispatch("/get_cluster_list", reqClusterList_http);
	regNumberDispatch(CMD_REQ_CLUSTER_CENTER_LOGIN, reqClusterCenterLogin);

	return 1;
}

#ifdef __cplusplus
}
#endif