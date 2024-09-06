#ifndef BOOT_SERVER_NET_CHANNEL_WEBSOCKET_H
#define BOOT_SERVER_NET_CHANNEL_WEBSOCKET_H

#include "net_channel_proc_imp.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll NetChannel_t* openNetListenerWebsocket(const char* ip, unsigned short port, FnNetChannelOnRecv_t fn, void* sche);

#ifdef __cplusplus
}
#endif

#endif
