#ifndef BOOT_SERVER_CONFIG_H
#define	BOOT_SERVER_CONFIG_H

#include "util/inc/sysapi/socket.h"

typedef struct {
	const char* protocol;
	int socktype;
	IPString_t ip;
	unsigned short port;
	int readcache_max_size;
} ConfigListenOption_t, ConfigConnectOption_t;

struct cJSON;
typedef struct Config_t {
	const ConfigListenOption_t* listen_options;
	unsigned int listen_options_cnt;
	const ConfigConnectOption_t* connect_options;
	unsigned int connect_options_cnt;
	IPString_t outer_ip;
	struct {
		const char* ident;
		ConfigListenOption_t listen_option;
	} clsnd;
	struct {
		const char* pathname;
		unsigned int maxfilesize;
	} log;
	int net_thread_cnt;
	const char* cluster_table_path;
	unsigned int rpc_fiber_stack_size;
	int once_rpc_timeout_items_maxcnt;
	int once_timeout_events_maxcnt;
	int once_handle_msg_maxcnt;
	int tcp_nodelay;
	int udp_cwndsize;
	int enqueue_timeout_msec;
	struct cJSON* cjson_root;
} Config_t;

#ifdef __cplusplus
extern "C" {
#endif

Config_t* initConfig(const char* path, Config_t* conf);
void resetConfig(Config_t* conf);

#ifdef __cplusplus
}
#endif

#endif // !CONFIG_H
