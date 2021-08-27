#include "util/inc/sysapi/socket.h"
#include "util/inc/sysapi/statistics.h"
#include "util/inc/crt/cJSON.h"
#include "util/inc/datastruct/memfunc.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

Config_t* initConfig(const char* path, Config_t* conf) {
	int res;
	cJSON* root = cJSON_ParseFromFile(NULL, path);
	if (!root) {
		return NULL;
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
				conf->clsnd.readcache_max_size = readcache_max_size->valueint;
			}
			conf->clsnd.id = id->valueint;
			conf->clsnd.socktype = if_string2socktype(socktype->valuestring);
			strcpy(conf->clsnd.ip, ip->valuestring);
			conf->clsnd.port = port->valueint;
		}

		cjson = cJSON_Field(root, "listen_options");
		if (cjson) {
			int i;
			cJSON* childnode;
			conf->listen_options_cnt = cJSON_Size(cjson);
			conf->listen_options = (ConfigListenOption_t*)malloc(sizeof(ConfigListenOption_t) * conf->listen_options_cnt);
			if (!conf->listen_options) {
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
				option_ptr = &conf->listen_options[i++];
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
			conf->listen_options_cnt = i;
		}

		cjson = cJSON_Field(root, "outer_ip");
		if (cjson) {
			strcpy(conf->outer_ip, cjson->valuestring);
		}

		cjson = cJSON_Field(root, "connect_options");
		if (cjson) {
			int i;
			cJSON* childnode;
			conf->connect_options_cnt = cJSON_Size(cjson);
			conf->connect_options = (ConfigConnectOption_t*)malloc(sizeof(ConfigConnectOption_t) * conf->connect_options_cnt);
			if (!conf->connect_options) {
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
				option_ptr = &conf->connect_options[i++];
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
			conf->connect_options_cnt = i;
		}

		cjson = cJSON_Field(root, "net_thread_cnt");
		if (cjson) {
			int net_thread_cnt = cjson->valueint;
			if (net_thread_cnt <= 0) {
				net_thread_cnt = processorCount();
			}
			conf->net_thread_cnt = net_thread_cnt;
		}
		else {
			conf->net_thread_cnt = 1;
		}

		cjson = cJSON_Field(root, "module_path");
		if (cjson) {
			conf->module_path = strdup(cjson->valuestring);
			if (!conf->module_path) {
				break;
			}
		}

		cjson = cJSON_Field(root, "cluster_table_path");
		if (cjson) {
			conf->cluster_table_path = strdup(cjson->valuestring);
			if (!conf->cluster_table_path) {
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
			conf->log.pathname = strdup(pathname->valuestring);
			if (!conf->log.pathname) {
				break;
			}
			maxfilesize_mb = cJSON_Field(cjson, "maxfilesize_mb");
			if (maxfilesize_mb && maxfilesize_mb->valueint > 0) {
				conf->log.maxfilesize = maxfilesize_mb->valueint * 1024 * 1024;
			}
			else {
				conf->log.maxfilesize = ~0;
			}
		}

		cjson = cJSON_Field(root, "rpc_fiber");
		if (cjson) {
			conf->rpc_fiber = cjson->valueint;
		}

		cjson = cJSON_Field(root, "rpc_fiber_stack_size_kb");
		if (cjson) {
			conf->rpc_fiber_stack_size = cjson->valueint * 1024;
		}
		else {
			conf->rpc_fiber_stack_size = 0x4000;
		}

		cjson = cJSON_Field(root, "rpc_async");
		if (cjson) {
			conf->rpc_async = cjson->valueint;
		}

		cjson = cJSON_Field(root, "tcp_nodelay");
		if (cjson) {
			conf->tcp_nodelay = cjson->valueint;
		}

		cjson = cJSON_Field(root, "udp_cwndsize");
		if (cjson) {
			conf->udp_cwndsize = cjson->valueint;
		}

		cjson = cJSON_Field(root, "enqueue_timeout_msec");
		if (cjson) {
			conf->enqueue_timeout_msec = cjson->valueint;
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
	free((char*)conf->module_path);
	conf->module_path = NULL;
	free((char*)conf->cluster_table_path);
	conf->cluster_table_path = NULL;
	conf->outer_ip[0] = 0;
	cJSON_Delete(conf->cjson_root);
	conf->cjson_root = NULL;
}

#ifdef __cplusplus
}
#endif
