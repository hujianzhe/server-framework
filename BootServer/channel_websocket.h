#ifndef BOOT_SERVER_CHANNEL_WEBSOCKET_H
#define BOOT_SERVER_CHANNEL_WEBSOCKET_H

#include "channel_proc_imp.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll ChannelBase_t* openListenerWebsocket(const char* ip, unsigned short port, FnChannelOnRecv_t fn, struct DataQueue_t* dq);

#ifdef __cplusplus
}
#endif

#endif