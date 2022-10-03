#ifndef BOOT_SERVER_CHANNEL_WEB_H
#define	BOOT_SERVER_CHANNEL_WEB_H

#include "channel_proc_imp.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll ChannelBase_t* openChannelHttp(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq);
__declspec_dll ChannelBase_t* openListenerHttp(const char* ip, unsigned short port, struct DataQueue_t* dq);

__declspec_dll ChannelBase_t* openListenerWebsocket(const char* ip, unsigned short port, FnChannelOnRecv_t fn, struct DataQueue_t* dq);

#ifdef __cplusplus
}
#endif

#endif // !BOOT_SERVER_CHANNEL_WEB_H
