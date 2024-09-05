#ifndef BOOT_SERVER_NET_CHANNEL_HTTP_H
#define	BOOT_SERVER_NET_CHANNEL_HTTP_H

#include "net_channel_proc_imp.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll NetChannel_t* openNetChannelHttpClient(const char* ip, unsigned short port, void* sche);
__declspec_dll NetChannel_t* openNetListenerHttp(const char* ip, unsigned short port, void* sche);

#ifdef __cplusplus
}
#endif

#endif