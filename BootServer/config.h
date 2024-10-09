#ifndef BOOT_SERVER_CONFIG_H
#define	BOOT_SERVER_CONFIG_H

#include "util/inc/sysapi/socket.h"

typedef struct {
	const char* protocol;
	int socktype;
	IPString_t ip;
	unsigned short port;
	int readcache_max_size;
	struct cJSON* cjson_node;
} ConfigListenOption_t;

typedef struct {
	const char* protocol;
	int socktype;
	IPString_t ip;
	unsigned short port;
	int readcache_max_size;
	struct cJSON* cjson_node;
	const char* user;
	size_t user_strlen;
	const char* password;
	size_t password_strlen;
} ConfigConnectOption_t;

struct cJSON;
typedef struct BootServerConfig_t {
	const ConfigListenOption_t* listen_options;
	unsigned int listen_options_cnt;
	const ConfigConnectOption_t* connect_options;
	unsigned int connect_options_cnt;
	IPString_t outer_ip;
	struct {
		const char* ident;
		size_t ident_strlen;
		struct cJSON* cjson_node;
	} clsnd;
	struct {
		const char* pathname;
		unsigned int maxfilesize;
		struct cJSON* cjson_node;
	} log;
	int net_thread_cnt;
	int task_thread_max_cnt;
	const char* cluster_table_path;
	unsigned int rpc_fiber_stack_size;
	int once_rpc_timeout_items_maxcnt;
	int once_timeout_events_maxcnt;
	int once_handle_msg_maxcnt;
	int tcp_nodelay;
	int udp_cwndsize;
	int enqueue_timeout_msec;
	struct cJSON* cjson_root;
} BootServerConfig_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll BootServerConfig_t* parseBootServerConfig(const char* path);
__declspec_dll void freeBootServerConfig(BootServerConfig_t* conf);

#ifdef __cplusplus
}
#endif

#endif // !CONFIG_H
