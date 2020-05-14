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

		cjson = cJSON_Field(root, "cluster");
		if (!cjson) {
			break;
		}
		else {
			cJSON* group_name, *ip, *port;
			group_name = cJSON_Field(cjson, "group_name");
			if (!group_name)
				break;
			ip = cJSON_Field(cjson, "ip");
			if (!ip)
				break;
			port = cJSON_Field(cjson, "port");
			if (!port)
				break;
			g_Config.cluster.group_name = strdup(group_name->valuestring);
			if (!g_Config.cluster.group_name)
				break;
			strcpy(g_Config.cluster.ip, ip->valuestring);
			g_Config.cluster.port = port->valueint;
		}

		cjson = cJSON_Field(root, "module_path");
		if (cjson && cjson->valuestring && cjson->valuestring[0]) {
			g_Config.module_path = strdup(cjson->valuestring);
			if (!g_Config.module_path) {
				break;
			}
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
				cJSON* socktype = cJSON_Field(childnode, "socktype");
				if (!protocol || !ipnode || !portnode) {
					continue;
				}
				option_ptr = &g_Config.listen_options[i++];
				option_ptr->protocol = strdup(protocol->valuestring);
				strcpy(option_ptr->ip, ipnode->valuestring);
				option_ptr->port = portnode->valueint;
				if (!socktype)
					option_ptr->socktype = SOCK_STREAM;
				else if (!strcmp(socktype->valuestring, "SOCK_STREAM"))
					option_ptr->socktype = SOCK_STREAM;
				else if (!strcmp(socktype->valuestring, "SOCK_DGRAM"))
					option_ptr->socktype = SOCK_DGRAM;
				else
					option_ptr->socktype = SOCK_STREAM;
			}
			g_Config.listen_options_cnt = i;
		}

		cjson = cJSON_Field(root, "outer_ip");
		if (cjson) {
			strcpy(g_Config.outer_ip, cjson->valuestring);
		}

		cjson = cJSON_Field(root, "connect_options");
		if (cjson) {
			int i;
			cJSON* childnode;
			g_Config.connect_options_cnt = cJSON_Size(cjson);
			g_Config.connect_options = (ConfigListenOption_t*)malloc(sizeof(ConfigListenOption_t) * g_Config.connect_options_cnt);
			if (!g_Config.connect_options) {
				break;
			}
			i = 0;
			for (childnode = cjson->child; childnode; childnode = childnode->next) {
				ConfigListenOption_t* option_ptr;
				cJSON* protocol = cJSON_Field(childnode, "protocol");
				cJSON* ipnode = cJSON_Field(childnode, "ip");
				cJSON* portnode = cJSON_Field(childnode, "port");
				cJSON* socktype = cJSON_Field(childnode, "socktype");
				if (!protocol || !ipnode || !portnode) {
					continue;
				}
				option_ptr = &g_Config.connect_options[i++];
				option_ptr->protocol = strdup(protocol->valuestring);
				strcpy(option_ptr->ip, ipnode->valuestring);
				option_ptr->port = portnode->valueint;
				if (!socktype)
					option_ptr->socktype = SOCK_STREAM;
				else if (!strcmp(socktype->valuestring, "SOCK_STREAM"))
					option_ptr->socktype = SOCK_STREAM;
				else if (!strcmp(socktype->valuestring, "SOCK_DGRAM"))
					option_ptr->socktype = SOCK_DGRAM;
				else
					option_ptr->socktype = SOCK_STREAM;
			}
			g_Config.connect_options_cnt = i;
		}

		cjson = cJSON_Field(root, "rpc_fiber");
		if (cjson) {
			g_Config.rpc_fiber = cjson->valueint;
		}

		cjson = cJSON_Field(root, "rpc_fiber_stack_size");
		if (cjson) {
			g_Config.rpc_fiber_stack_size = cjson->valueint;
		}
		else {
			g_Config.rpc_fiber_stack_size = 0x4000;
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

	for (i = 0; i < g_Config.connect_options_cnt; ++i) {
		free((char*)g_Config.connect_options[i].protocol);
	}
	free(g_Config.connect_options);
	g_Config.connect_options = NULL;
	g_Config.connect_options_cnt = 0;
	
	free((char*)g_Config.cluster.group_name);
	g_Config.cluster.group_name = NULL;
	free((char*)g_Config.module_path);
	g_Config.module_path = NULL;
	g_Config.outer_ip[0] = 0;
}

Config_t* ptr_g_Config(void) { return &g_Config; }

#ifdef __cplusplus
}
#endif
