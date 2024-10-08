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

static ConfigListenOption_t* parse_listen_option(cJSON* cjson, ConfigListenOption_t* option_ptr) {
	cJSON* protocol = cJSON_GetField(cjson, "protocol");
	cJSON* ipnode = cJSON_GetField(cjson, "ip");
	cJSON* portnode = cJSON_GetField(cjson, "port");
	cJSON* socktype = cJSON_GetField(cjson, "socktype");
	cJSON* readcache_max_size = cJSON_GetField(cjson, "readcache_max_size");

	if (!protocol || !ipnode || !portnode) {
		return NULL;
	}
	option_ptr->protocol = strdup(cJSON_GetStringPtr(protocol));
	if (!option_ptr->protocol) {
		return NULL;
	}
	strcpy(option_ptr->ip, cJSON_GetStringPtr(ipnode));
	option_ptr->port = cJSON_GetInteger(portnode);
	if (!socktype) {
		option_ptr->socktype = SOCK_STREAM;
	}
	else {
		option_ptr->socktype = if_string2socktype(cJSON_GetStringPtr(socktype));
		if (0 == option_ptr->socktype) {
			return NULL;
		}
	}
	if (readcache_max_size && cJSON_GetInteger(readcache_max_size) > 0) {
		option_ptr->readcache_max_size = cJSON_GetInteger(readcache_max_size);
	}
	else {
		option_ptr->readcache_max_size = 0;
	}
	return option_ptr;
}

static ConfigConnectOption_t* parse_connect_option(cJSON* cjson, ConfigConnectOption_t* option_ptr) {
	cJSON* protocol = cJSON_GetField(cjson, "protocol");
	cJSON* ipnode = cJSON_GetField(cjson, "ip");
	cJSON* portnode = cJSON_GetField(cjson, "port");
	cJSON* socktype = cJSON_GetField(cjson, "socktype");
	cJSON* readcache_max_size = cJSON_GetField(cjson, "readcache_max_size");
	cJSON* user = cJSON_GetField(cjson, "user");
	cJSON* password = cJSON_GetField(cjson, "password");

	if (!protocol || !ipnode || !portnode) {
		return NULL;
	}
	option_ptr->protocol = strdup(cJSON_GetStringPtr(protocol));
	if (!option_ptr->protocol) {
		return NULL;
	}
	strcpy(option_ptr->ip, cJSON_GetStringPtr(ipnode));
	option_ptr->port = cJSON_GetInteger(portnode);
	if (!socktype) {
		option_ptr->socktype = SOCK_STREAM;
	}
	else {
		option_ptr->socktype = if_string2socktype(cJSON_GetStringPtr(socktype));
		if (0 == option_ptr->socktype) {
			return NULL;
		}
	}
	if (readcache_max_size && cJSON_GetInteger(readcache_max_size) > 0) {
		option_ptr->readcache_max_size = cJSON_GetInteger(readcache_max_size);
	}
	else {
		option_ptr->readcache_max_size = 0;
	}
	if (user) {
		option_ptr->user = strdup(cJSON_GetStringPtr(user));
		if (!option_ptr->user) {
			return NULL;
		}
		option_ptr->user_strlen = cJSON_GetStringLength(user);
	}
	if (password) {
		option_ptr->password = strdup(cJSON_GetStringPtr(password));
		if (!option_ptr->password) {
			return NULL;
		}
		option_ptr->password_strlen = cJSON_GetStringLength(password);
	}
	return option_ptr;
}

BootServerConfig_t* parseBootServerConfig(const char* path) {
	BootServerConfig_t* conf;
	cJSON* cjson;
	cJSON *ident;
	cJSON* root = cJSON_FromFile(path);
	if (!root) {
		return NULL;
	}
	conf = (BootServerConfig_t*)calloc(1, sizeof(BootServerConfig_t));
	if (!conf) {
		cJSON_Delete(root);
		return NULL;
	}
	conf->cjson_root = root;

	cjson = cJSON_GetField(root, "cluster");
	if (!cjson) {
		goto err;
	}
	ident = cJSON_GetField(cjson, "ident");
	if (!ident) {
		goto err;
	}
	conf->clsnd.ident = strdup(cJSON_GetStringPtr(ident));
	if (!conf->clsnd.ident) {
		goto err;
	}
	conf->clsnd.ident_strlen = cJSON_GetStringLength(ident);

	cjson = cJSON_GetField(root, "outer_ip");
	if (cjson) {
		strcpy(conf->outer_ip, cJSON_GetStringPtr(cjson));
	}

	cjson = cJSON_GetField(root, "listen_options");
	if (cjson) {
		cJSON* childnode;
		size_t n = cJSON_ChildNum(cjson);
		ConfigListenOption_t* listen_options;

		listen_options = (ConfigListenOption_t*)malloc(sizeof(ConfigListenOption_t) * n);
		if (!listen_options) {
			goto err;
		}
		conf->listen_options = listen_options;
		conf->listen_options_cnt = 0;
		for (childnode = cjson->child; childnode; childnode = childnode->next) {
			ConfigListenOption_t* option_ptr = &listen_options[conf->listen_options_cnt++];
			memset(option_ptr, 0, sizeof(*option_ptr));
			if (!parse_listen_option(childnode, option_ptr)) {
				goto err;
			}
		}
	}

	cjson = cJSON_GetField(root, "connect_options");
	if (cjson) {
		cJSON* childnode;
		ConfigConnectOption_t* connect_options;
		size_t n = cJSON_ChildNum(cjson);

		connect_options = (ConfigConnectOption_t*)malloc(sizeof(ConfigConnectOption_t) * n);
		if (!connect_options) {
			goto err;
		}
		conf->connect_options = connect_options;
		conf->connect_options_cnt = 0;
		for (childnode = cjson->child; childnode; childnode = childnode->next) {
			ConfigConnectOption_t* option_ptr = &connect_options[conf->connect_options_cnt++];
			memset(option_ptr, 0, sizeof(*option_ptr));
			if (!parse_connect_option(childnode, option_ptr)) {
				goto err;
			}
		}
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

	cjson = cJSON_GetField(root, "task_thread_max_cnt");
	if (cjson) {
		int task_thread_max_cnt = cJSON_GetInteger(cjson);
		if (task_thread_max_cnt <= 0) {
			task_thread_max_cnt = processorCount();
		}
		conf->task_thread_max_cnt = task_thread_max_cnt;
	}
	else {
		conf->task_thread_max_cnt = 1;
	}

	cjson = cJSON_GetField(root, "cluster_table_path");
	if (cjson) {
		conf->cluster_table_path = strdup(cJSON_GetStringPtr(cjson));
		if (!conf->cluster_table_path) {
			goto err;
		}
	}

	cjson = cJSON_GetField(root, "log");
	if (cjson) {
		cJSON* pathname, *maxfilesize_mb;
		pathname = cJSON_GetField(cjson, "pathname");
		if (!pathname) {
			goto err;
		}
		conf->log.pathname = strdup(cJSON_GetStringPtr(pathname));
		if (!conf->log.pathname) {
			goto err;
		}
		maxfilesize_mb = cJSON_GetField(cjson, "maxfilesize_mb");
		if (maxfilesize_mb && cJSON_GetInteger(maxfilesize_mb) > 0) {
			conf->log.maxfilesize = cJSON_GetInteger(maxfilesize_mb) * 1024 * 1024;
		}
		else {
			conf->log.maxfilesize = ~0;
		}
	}

	cjson = cJSON_GetField(root, "rpc_fiber_stack_size_kb");
	if (cjson) {
		conf->rpc_fiber_stack_size = cJSON_GetInteger(cjson) * 1024;
	}
	else {
		conf->rpc_fiber_stack_size = 0x4000;
	}

	conf->once_rpc_timeout_items_maxcnt = 32;
	cjson = cJSON_GetField(root, "once_rpc_timeout_items_maxcnt");
	if (cjson) {
		int val = cJSON_GetInteger(cjson);
		if (val > 0) {
			conf->once_rpc_timeout_items_maxcnt = val;
		}
	}

	conf->once_timeout_events_maxcnt = 32;
	cjson = cJSON_GetField(root, "once_timeout_events_maxcnt");
	if (cjson) {
		int val = cJSON_GetInteger(cjson);
		if (val > 0) {
			conf->once_timeout_events_maxcnt = val;
		}
	}

	conf->once_handle_msg_maxcnt = -1;
	cjson = cJSON_GetField(root, "once_handle_msg_maxcnt");
	if (cjson) {
		int val = cJSON_GetInteger(cjson);
		if (val > 0) {
			conf->once_handle_msg_maxcnt = val;
		}
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

	return conf;
err:
	freeBootServerConfig(conf);
	return NULL;
}

void freeBootServerConfig(BootServerConfig_t* conf) {
	unsigned int i;
	if (!conf) {
		return;
	}
	for (i = 0; i < conf->listen_options_cnt; ++i) {
		free((char*)conf->listen_options[i].protocol);
	}
	free((void*)conf->listen_options);
	for (i = 0; i < conf->connect_options_cnt; ++i) {
		const ConfigConnectOption_t* option_ptr = conf->connect_options + i;
		free((char*)option_ptr->protocol);
		free((char*)option_ptr->user);
		free((char*)option_ptr->password);
	}
	free((void*)conf->connect_options);
	free((char*)conf->clsnd.ident);
	free((char*)conf->log.pathname);
	free((char*)conf->cluster_table_path);
	cJSON_Delete(conf->cjson_root);
	free(conf);
}

#ifdef __cplusplus
}
#endif
