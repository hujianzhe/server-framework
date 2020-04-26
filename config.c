#include "util/inc/sysapi/socket.h"
#include "util/inc/component/cJSON.h"
#include "util/inc/datastruct/memfunc.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>

Config_t g_Config;

#ifdef __cplusplus
extern "C" {
#endif

int initConfig(const char* path) {
	int res = 0;
	cJSON* root, *cjson;
	do {
		root = cJSON_ParseFromFile(NULL, path);
		if (!root) {
			break;
		}

		cjson = cJSON_Field(root, "cluster_name");
		if (cjson && cjson->valuestring && cjson->valuestring[0]) {
			g_Config.cluster_name = strdup(cjson->valuestring);
			if (!g_Config.cluster_name) {
				break;
			}
		}
		else {
			break;
		}

		cjson = cJSON_Field(root, "listen_options");
		if (cjson) {
			int i;
			cJSON* childnode;
			g_Config.listen_options_cnt = cJSON_Size(cjson);
			g_Config.listen_options = (ConfigListenOption_t*)malloc(sizeof(ConfigListenOption_t) * g_Config.listen_options_cnt);
			if (!g_Config.listen_options) {
				break;
			}
			i = 0;
			for (childnode = cjson->child; childnode; childnode = childnode->next) {
				ConfigListenOption_t* option_ptr;
				cJSON* protocol = cJSON_Field(childnode, "protocol");
				cJSON* ipnode = cJSON_Field(childnode, "ip");
				cJSON* portnode = cJSON_Field(childnode, "port");
				if (!protocol || !ipnode || !portnode) {
					continue;
				}
				option_ptr = &g_Config.listen_options[i++];
				option_ptr->protocol = strdup(protocol->valuestring);
				strcpy(option_ptr->ip, ipnode->valuestring);
				option_ptr->port = portnode->valueint;
			}
			g_Config.listen_options_cnt = i;
		}

		cjson = cJSON_Field(root, "ipv6_enable");
		if (cjson) {
			g_Config.domain = cjson->valueint ? AF_INET6 : AF_INET;
		}
		else {
			g_Config.domain = AF_INET;
		}

		cjson = cJSON_Field(root, "socktype");
		if (cjson) {
			if (!strcmp(cjson->valuestring, "SOCK_STREAM"))
				g_Config.socktype = SOCK_STREAM;
			else if (!strcmp(cjson->valuestring, "SOCK_DGRAM"))
				g_Config.socktype = SOCK_DGRAM;
			else
				break;
		}
		else {
			g_Config.socktype = SOCK_STREAM;
		}

		cjson = cJSON_Field(root, "outer_ip");
		if (cjson) {
			strcpy(g_Config.outer_ip, cjson->valuestring);
		}

		cjson = cJSON_Field(root, "center_addr");
		if (cjson) {
			cJSON* sub_cjson = cJSON_Field(cjson, "ip");
			if (sub_cjson)
				strcpy(g_Config.center_attr.ip, sub_cjson->valuestring);
			sub_cjson = cJSON_Field(cjson, "port");
			if (sub_cjson)
				g_Config.center_attr.port = sub_cjson->valueint;
			sub_cjson = cJSON_Field(cjson, "socktype");
			if (sub_cjson) {
				if (!strcmp(sub_cjson->valuestring, "SOCK_STREAM"))
					g_Config.center_attr.socktype = SOCK_STREAM;
				else if (!strcmp(sub_cjson->valuestring, "SOCK_DGRAM"))
					g_Config.center_attr.socktype = SOCK_DGRAM;
				else
					break;
			}
			else {
				g_Config.center_attr.socktype = SOCK_STREAM;
			}
		}

		cjson = cJSON_Field(root, "rpc_fiber");
		if (cjson) {
			g_Config.rpc_fiber = cjson->valueint;
		}

		cjson = cJSON_Field(root, "rpc_async");
		if (cjson) {
			g_Config.rpc_async = cjson->valueint;
		}

		cjson = cJSON_Field(root, "tcp_nodelay");
		if (cjson) {
			g_Config.tcp_nodelay = cjson->valueint;
		}

		cjson = cJSON_Field(root, "udp_cwndsize");
		if (cjson) {
			g_Config.udp_cwndsize = cjson->valueint;
		}

		res = 1;
	} while (0);
	cJSON_Delete(root);
	return res;
}

void freeConfig(void) {
	unsigned int i;
	for (i = 0; i < g_Config.listen_options_cnt; ++i) {
		free((char*)g_Config.listen_options[i].protocol);
	}
	free(g_Config.listen_options);
	g_Config.listen_options = NULL;
	g_Config.listen_options_cnt = 0;
	free((char*)g_Config.cluster_name);
	g_Config.cluster_name = NULL;
	g_Config.outer_ip[0] = 0;
	g_Config.center_attr.ip[0] = 0;
}

#ifdef __cplusplus
}
#endif
