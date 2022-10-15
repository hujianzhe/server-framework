#ifndef BOOT_SERVER_CHANNEL_HTTP_H
#define	BOOT_SERVER_CHANNEL_HTTP_H

#include "channel_proc_imp.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll ChannelBase_t* openChannelHttp(int flag, FD_t fd, const struct sockaddr* addr, struct DataQueue_t* dq);
__declspec_dll ChannelBase_t* openListenerHttp(const char* ip, unsigned short port, struct DataQueue_t* dq);

#ifdef __cplusplus
}
#endif

#endif // !BOOT_SERVER_CHANNEL_HTTP_H
