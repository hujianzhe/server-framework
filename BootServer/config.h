#ifndef BOOT_SERVER_CONFIG_H
#define	BOOT_SERVER_CONFIG_H

#include "util/inc/sysapi/socket.h"

typedef struct BootServerConfigNetChannelOption_t {
	int socktype;
	IPString_t ip;
	unsigned short port;
	int readcache_max_size;
	int sendcache_max_size;
	int command_timeout_msec;
	int heartbeat_timeout_msec;
	unsigned short heartbeat_max_times;
} BootServerConfigNetChannelOption_t;

typedef struct BootServerConfigListenOption_t {
	const char* protocol;
	int backlog;
	BootServerConfigNetChannelOption_t channel_opt;
	struct cJSON* cjson_node;
} BootServerConfigListenOption_t;

typedef struct BootServerConfigConnectOption_t {
	const char* protocol;
	int connect_timeout_msec;
	const char* user;
	size_t user_strlen;
	const char* password;
	size_t password_strlen;
	BootServerConfigNetChannelOption_t channel_opt;
	struct cJSON* cjson_node;
} BootServerConfigConnectOption_t;

typedef struct BootServerConfigSchedulerOption_t {
	int net_thread_cnt;
	unsigned int task_thread_stack_size;
	unsigned int fiber_stack_size;
	int once_handle_cnt;
	struct cJSON* cjson_node;
} BootServerConfigSchedulerOption_t;

typedef struct BootServerConfigLoggerOption_t {
	const char* key;
	const char* base_path;
	int async_output;
	struct cJSON* cjson_node;
} BootServerConfigLoggerOption_t;

typedef struct BootServerConfigClusterNodeOption_t {
	const char* ident;
	size_t ident_strlen;
	struct cJSON* cjson_node;
} BootServerConfigClusterNodeOption_t;

struct cJSON;
typedef struct BootServerConfig_t {
	const BootServerConfigListenOption_t* listen_options;
	unsigned int listen_options_cnt;
	const BootServerConfigConnectOption_t* connect_options;
	unsigned int connect_options_cnt;
	BootServerConfigClusterNodeOption_t clsnd;
	BootServerConfigLoggerOption_t* log_options;
	unsigned int log_options_cnt;
	BootServerConfigSchedulerOption_t sche;
	IPString_t outer_ip;
	const char* cluster_table_path;
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
