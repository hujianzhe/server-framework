#include "util/inc/sysapi/socket.h"
#include "util/inc/sysapi/statistics.h"
#include "util/inc/crt/cJSON.h"
#include "util/inc/datastruct/memfunc.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>

Config_t g_Config;

#ifdef __cplusplus
extern "C" {
#endif

int initConfig(const char* path) {
	int res;
	cJSON* root = cJSON_ParseFromFile(NULL, path);
	if (!root) {
		return 0;
	}
	res = 0;
	do {
		cJSON* cjson;
		cjson = cJSON_Field(root, "cluster");
		if (!cjson) {
			break;
		}
		else {
			cJSON *id, *socktype, *ip, *port, *readcache_max_size;
			id = cJSON_Field(cjson, "id");
			if (!id) {
				break;
			}
			socktype = cJSON_Field(cjson, "socktype");
			if (!socktype) {
				break;
			}
			ip = cJSON_Field(cjson, "ip");
			if (!ip) {
				break;
			}
			port = cJSON_Field(cjson, "port");
			if (!port) {
				break;
			}
			readcache_max_size = cJSON_Field(cjson, "readcache_max_size");
			if (readcache_max_size && readcache_max_size->valueint > 0) {
				g_Config.clsnd.readcache_max_size = readcache_max_size->valueint;
			}
			g_Config.clsnd.id = id->valueint;
			g_Config.clsnd.socktype = if_string2socktype(socktype->valuestring);
			strcpy(g_Config.clsnd.ip, ip->valuestring);
			g_Config.clsnd.port = port->valueint;
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
				cJSON* readcache_max_size = cJSON_Field(childnode, "readcache_max_size");
				if (!protocol || !ipnode || !portnode) {
					continue;
				}
				option_ptr = &g_Config.listen_options[i++];
				option_ptr->protocol = strdup(protocol->valuestring);
				strcpy(option_ptr->ip, ipnode->valuestring);
				option_ptr->port = portnode->valueint;
				if (!socktype) {
					option_ptr->socktype = SOCK_STREAM;
				}
				else {
					option_ptr->socktype = if_string2socktype(socktype->valuestring);
				}
				if (readcache_max_size && readcache_max_size->valueint > 0) {
					option_ptr->readcache_max_size = readcache_max_size->valueint;
				}
				else {
					option_ptr->readcache_max_size = 0;
				}
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
			g_Config.connect_options = (ConfigConnectOption_t*)malloc(sizeof(ConfigConnectOption_t) * g_Config.connect_options_cnt);
			if (!g_Config.connect_options) {
				break;
			}
			i = 0;
			for (childnode = cjson->child; childnode; childnode = childnode->next) {
				ConfigConnectOption_t* option_ptr;
				cJSON* protocol = cJSON_Field(childnode, "protocol");
				cJSON* ipnode = cJSON_Field(childnode, "ip");
				cJSON* portnode = cJSON_Field(childnode, "port");
				cJSON* socktype = cJSON_Field(childnode, "socktype");
				cJSON* readcache_max_size = cJSON_Field(childnode, "readcache_max_size");
				if (!protocol || !ipnode || !portnode) {
					continue;
				}
				option_ptr = &g_Config.connect_options[i++];
				option_ptr->protocol = strdup(protocol->valuestring);
				strcpy(option_ptr->ip, ipnode->valuestring);
				option_ptr->port = portnode->valueint;
				if (!socktype) {
					option_ptr->socktype = SOCK_STREAM;
				}
				else {
					option_ptr->socktype = if_string2socktype(socktype->valuestring);
				}
				if (readcache_max_size && readcache_max_size->valueint > 0) {
					option_ptr->readcache_max_size = readcache_max_size->valueint;
				}
				else {
					option_ptr->readcache_max_size = 0;
				}
			}
			g_Config.connect_options_cnt = i;
		}

		cjson = cJSON_Field(root, "net_thread_cnt");
		if (cjson) {
			int net_thread_cnt = cjson->valueint;
			if (net_thread_cnt <= 0) {
				net_thread_cnt = processorCount();
			}
			g_Config.net_thread_cnt = net_thread_cnt;
		}
		else {
			g_Config.net_thread_cnt = 1;
		}

		cjson = cJSON_Field(root, "module_path");
		if (cjson) {
			g_Config.module_path = strdup(cjson->valuestring);
			if (!g_Config.module_path) {
				break;
			}
		}

		cjson = cJSON_Field(root, "cluster_table_path");
		if (cjson) {
			g_Config.cluster_table_path = strdup(cjson->valuestring);
			if (!g_Config.cluster_table_path) {
				break;
			}
		}

		cjson = cJSON_Field(root, "log");
		if (!cjson) {
			break;
		}
		else {
			cJSON* pathname, *maxfilesize_mb;
			pathname = cJSON_Field(cjson, "pathname");
			if (!pathname) {
				break;
			}
			g_Config.log.pathname = strdup(pathname->valuestring);
			if (!g_Config.log.pathname) {
				break;
			}
			maxfilesize_mb = cJSON_Field(cjson, "maxfilesize_mb");
			if (maxfilesize_mb && maxfilesize_mb->valueint > 0) {
				g_Config.log.maxfilesize = maxfilesize_mb->valueint * 1024 * 1024;
			}
			else {
				g_Config.log.maxfilesize = ~0;
			}
		}

		cjson = cJSON_Field(root, "rpc_fiber");
		if (cjson) {
			g_Config.rpc_fiber = cjson->valueint;
		}

		cjson = cJSON_Field(root, "rpc_fiber_stack_size_kb");
		if (cjson) {
			g_Config.rpc_fiber_stack_size = cjson->valueint * 1024;
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

		cjson = cJSON_Field(root, "enqueue_timeout_msec");
		if (cjson) {
			g_Config.enqueue_timeout_msec = cjson->valueint;
		}

		res = 1;
	} while (0);
	if (res) {
		g_Config.cjson_root = root;
	}
	else {
		cJSON_Delete(root);
	}
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
	free((char*)g_Config.log.pathname);
	g_Config.log.pathname = NULL;
	free((char*)g_Config.module_path);
	g_Config.module_path = NULL;
	free((char*)g_Config.cluster_table_path);
	g_Config.cluster_table_path = NULL;
	g_Config.outer_ip[0] = 0;
	cJSON_Delete(g_Config.cjson_root);
	g_Config.cjson_root = NULL;
}

Config_t* ptr_g_Config(void) { return &g_Config; }

#ifdef __cplusplus
}
#endif
