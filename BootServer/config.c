#include "util/inc/sysapi/socket.h"
#include "util/inc/sysapi/statistics.h"
#include "util/inc/crt/json.h"
#include "util/inc/datastruct/memfunc.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

Config_t* initConfig(const char* path, Config_t* conf) {
	int res;
	cJSON* root = cJSON_FromFile(path);
	if (!root) {
		return NULL;
	}
	res = 0;
	do {
		cJSON* cjson;
		cjson = cJSON_GetField(root, "cluster");
		if (!cjson) {
			break;
		}
		else {
			cJSON *id, *socktype, *ip, *port, *readcache_max_size;
			id = cJSON_GetField(cjson, "id");
			if (!id) {
				break;
			}
			socktype = cJSON_GetField(cjson, "socktype");
			if (!socktype) {
				break;
			}
			ip = cJSON_GetField(cjson, "ip");
			if (!ip) {
				break;
			}
			port = cJSON_GetField(cjson, "port");
			if (!port) {
				break;
			}
			readcache_max_size = cJSON_GetField(cjson, "readcache_max_size");
			if (readcache_max_size && cJSON_GetInteger(readcache_max_size) > 0) {
				conf->clsnd.readcache_max_size = cJSON_GetInteger(readcache_max_size);
			}
			conf->clsnd.id = cJSON_GetInteger(id);
			conf->clsnd.socktype = if_string2socktype(cJSON_GetStringPtr(socktype));
			strcpy(conf->clsnd.ip, cJSON_GetStringPtr(ip));
			conf->clsnd.port = cJSON_GetInteger(port);
		}

		cjson = cJSON_GetField(root, "listen_options");
		if (cjson) {
			int i;
			cJSON* childnode;
			conf->listen_options_cnt = cJSON_ChildNum(cjson);
			conf->listen_options = (ConfigListenOption_t*)malloc(sizeof(ConfigListenOption_t) * conf->listen_options_cnt);
			if (!conf->listen_options) {
				break;
			}
			i = 0;
			for (childnode = cjson->child; childnode; childnode = childnode->next) {
				ConfigListenOption_t* option_ptr;
				cJSON* protocol = cJSON_GetField(childnode, "protocol");
				cJSON* ipnode = cJSON_GetField(childnode, "ip");
				cJSON* portnode = cJSON_GetField(childnode, "port");
				cJSON* socktype = cJSON_GetField(childnode, "socktype");
				cJSON* readcache_max_size = cJSON_GetField(childnode, "readcache_max_size");
				if (!protocol || !ipnode || !portnode) {
					continue;
				}
				option_ptr = &conf->listen_options[i++];
				option_ptr->protocol = strdup(cJSON_GetStringPtr(protocol));
				strcpy(option_ptr->ip, cJSON_GetStringPtr(ipnode));
				option_ptr->port = cJSON_GetInteger(portnode);
				if (!socktype) {
					option_ptr->socktype = SOCK_STREAM;
				}
				else {
					option_ptr->socktype = if_string2socktype(cJSON_GetStringPtr(socktype));
				}
				if (readcache_max_size && cJSON_GetInteger(readcache_max_size) > 0) {
					option_ptr->readcache_max_size = cJSON_GetInteger(readcache_max_size);
				}
				else {
					option_ptr->readcache_max_size = 0;
				}
			}
			conf->listen_options_cnt = i;
		}

		cjson = cJSON_GetField(root, "outer_ip");
		if (cjson) {
			strcpy(conf->outer_ip, cJSON_GetStringPtr(cjson));
		}

		cjson = cJSON_GetField(root, "connect_options");
		if (cjson) {
			int i;
			cJSON* childnode;
			conf->connect_options_cnt = cJSON_ChildNum(cjson);
			conf->connect_options = (ConfigConnectOption_t*)malloc(sizeof(ConfigConnectOption_t) * conf->connect_options_cnt);
			if (!conf->connect_options) {
				break;
			}
			i = 0;
			for (childnode = cjson->child; childnode; childnode = childnode->next) {
				ConfigConnectOption_t* option_ptr;
				cJSON* protocol = cJSON_GetField(childnode, "protocol");
				cJSON* ipnode = cJSON_GetField(childnode, "ip");
				cJSON* portnode = cJSON_GetField(childnode, "port");
				cJSON* socktype = cJSON_GetField(childnode, "socktype");
				cJSON* readcache_max_size = cJSON_GetField(childnode, "readcache_max_size");
				if (!protocol || !ipnode || !portnode) {
					continue;
				}
				option_ptr = &conf->connect_options[i++];
				option_ptr->protocol = strdup(cJSON_GetStringPtr(protocol));
				strcpy(option_ptr->ip, cJSON_GetStringPtr(ipnode));
				option_ptr->port = cJSON_GetInteger(portnode);
				if (!socktype) {
					option_ptr->socktype = SOCK_STREAM;
				}
				else {
					option_ptr->socktype = if_string2socktype(cJSON_GetStringPtr(socktype));
				}
				if (readcache_max_size && cJSON_GetInteger(readcache_max_size) > 0) {
					option_ptr->readcache_max_size = cJSON_GetInteger(readcache_max_size);
				}
				else {
					option_ptr->readcache_max_size = 0;
				}
			}
			conf->connect_options_cnt = i;
		}

		cjson = cJSON_GetField(root, "net_thread_cnt");
		if (cjson) {
			int net_thread_cnt = cJSON_GetInteger(cjson);
			if (net_thread_cnt <= 0) {
				net_thread_cnt = processorCount();
			}
			conf->net_thread_cnt = net_thread_cnt;
		}
		else {
			conf->net_thread_cnt = 1;
		}

		cjson = cJSON_GetField(root, "cluster_table_path");
		if (cjson) {
			conf->cluster_table_path = strdup(cJSON_GetStringPtr(cjson));
			if (!conf->cluster_table_path) {
				break;
			}
		}

		cjson = cJSON_GetField(root, "log");
		if (!cjson) {
			break;
		}
		else {
			cJSON* pathname, *maxfilesize_mb;
			pathname = cJSON_GetField(cjson, "pathname");
			if (!pathname) {
				break;
			}
			conf->log.pathname = strdup(cJSON_GetStringPtr(pathname));
			if (!conf->log.pathname) {
				break;
			}
			maxfilesize_mb = cJSON_GetField(cjson, "maxfilesize_mb");
			if (maxfilesize_mb && cJSON_GetInteger(maxfilesize_mb) > 0) {
				conf->log.maxfilesize = cJSON_GetInteger(maxfilesize_mb) * 1024 * 1024;
			}
			else {
				conf->log.maxfilesize = ~0;
			}
		}

		cjson = cJSON_GetField(root, "rpc_fiber");
		if (cjson) {
			conf->rpc_fiber = cJSON_GetInteger(cjson);
		}

		cjson = cJSON_GetField(root, "rpc_fiber_stack_size_kb");
		if (cjson) {
			conf->rpc_fiber_stack_size = cJSON_GetInteger(cjson) * 1024;
		}
		else {
			conf->rpc_fiber_stack_size = 0x4000;
		}

		cjson = cJSON_GetField(root, "rpc_async");
		if (cjson) {
			conf->rpc_async = cJSON_GetInteger(cjson);
		}

		cjson = cJSON_GetField(root, "tcp_nodelay");
		if (cjson) {
			conf->tcp_nodelay = cJSON_GetInteger(cjson);
		}

		cjson = cJSON_GetField(root, "udp_cwndsize");
		if (cjson) {
			conf->udp_cwndsize = cJSON_GetInteger(cjson);
		}

		cjson = cJSON_GetField(root, "enqueue_timeout_msec");
		if (cjson) {
			conf->enqueue_timeout_msec = cJSON_GetInteger(cjson);
		}

		res = 1;
	} while (0);
	if (res) {
		conf->cjson_root = root;
		return conf;
	}
	else {
		cJSON_Delete(root);
		return NULL;
	}
}

void freeConfig(Config_t* conf) {
	unsigned int i;
	for (i = 0; i < conf->listen_options_cnt; ++i) {
		free((char*)conf->listen_options[i].protocol);
	}
	free(conf->listen_options);
	conf->listen_options = NULL;
	conf->listen_options_cnt = 0;

	for (i = 0; i < conf->connect_options_cnt; ++i) {
		free((char*)conf->connect_options[i].protocol);
	}
	free(conf->connect_options);
	conf->connect_options = NULL;
	conf->connect_options_cnt = 0;
	free((char*)conf->log.pathname);
	conf->log.pathname = NULL;
	free((char*)conf->cluster_table_path);
	conf->cluster_table_path = NULL;
	conf->outer_ip[0] = 0;
	cJSON_Delete(conf->cjson_root);
	conf->cjson_root = NULL;
}

#ifdef __cplusplus
}
#endif
