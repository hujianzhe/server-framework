#ifndef CONFIG_H
#define	CONFIG_H

#include "util/inc/sysapi/socket.h"

typedef struct ConfigListenOption_t {
	const char* protocol;
	IPString_t ip;
	unsigned short port;
} ConfigListenOption_t;

typedef struct Config_t {
	int domain;
	int socktype;
	ConfigListenOption_t* listen_options;
	unsigned int listen_options_cnt;
	IPString_t outer_ip;
	const char* cluster_name;
	int rpc_fiber;
	int rpc_async;
	int tcp_nodelay;
	int udp_cwndsize;
	struct {
		int socktype;
		IPString_t ip;
		unsigned short port;
	} center_attr;
} Config_t;

extern Config_t g_Config;

#ifdef __cplusplus
extern "C" {
#endif

int initConfig(const char* path);
void freeConfig(void);

#ifdef __cplusplus
}
#endif

#endif // !CONFIG_H
