#ifndef CONFIG_H
#define	CONFIG_H

#include "util/inc/sysapi/socket.h"

typedef struct {
	const char* protocol;
	int socktype;
	IPString_t ip;
	unsigned short port;
} ConfigListenOption_t, ConfigConnectOption_t;

typedef struct Config_t {
	ConfigListenOption_t* listen_options;
	unsigned int listen_options_cnt;
	ConfigConnectOption_t* connect_options;
	unsigned int connect_options_cnt;
	IPString_t outer_ip;
	struct {
		int socktype;
		IPString_t ip;
		unsigned short port;
	} cluster;
	const char* module_path;
	int rpc_fiber;
	unsigned int rpc_fiber_stack_size;
	int rpc_async;
	int tcp_nodelay;
	int udp_cwndsize;
} Config_t;

extern Config_t g_Config;

#ifdef __cplusplus
extern "C" {
#endif

int initConfig(const char* path);
void freeConfig(void);
__declspec_dllexport Config_t* ptr_g_Config(void);

#ifdef __cplusplus
}
#endif

#endif // !CONFIG_H
