#ifndef CONFIG_H
#define	CONFIG_H

#include "util/inc/sysapi/socket.h"

typedef struct Config_t {
	int domain;
	int socktype;
	IPString_t local_ip;
	IPString_t listen_ip;
	IPString_t outer_ip;
	unsigned short* port;
	unsigned int portcnt;
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
