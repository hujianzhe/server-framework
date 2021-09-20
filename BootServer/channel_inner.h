#ifndef BOOT_SERVER_CHANNEL_INNER_H
#define	BOOT_SERVER_CHANNEL_INNER_H

#include "channel_proc_imp.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll Channel_t* openChannelInner(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq);
__declspec_dll Channel_t* openListenerInner(int socktype, const char* ip, unsigned short port, struct DataQueue_t* dq);

#ifdef __cplusplus
}
#endif

#endif // !BOOT_SERVER_CHANNEL_INNER_H
