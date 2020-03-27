#ifndef CONFIG_H
#define	CONFIG_H

#include "util/inc/sysapi/socket.h"

typedef struct MQConfig_t {
	int domain;
	int socktype;
	IPString_t local_ip;
	IPString_t listen_ip;
	IPString_t outer_ip;
	unsigned short* port;
	unsigned int portcnt;
	int timer_interval_msec;
	const char* cluster_name;
	int cluster_id;
	int use_fiber;
	struct {
		int socktype;
		IPString_t ip;
		unsigned short port;
	} center_attr;
} MQConfig_t;

extern MQConfig_t g_Config;

#ifdef __cplusplus
extern "C" {
#endif

int initConfig(const char* path);
void freeConfig(void);

#ifdef __cplusplus
}
#endif

#endif // !CONFIG_H
