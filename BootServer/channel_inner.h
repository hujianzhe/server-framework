#ifndef BOOT_SERVER_CHANNEL_INNER_H
#define	BOOT_SERVER_CHANNEL_INNER_H

#include "channel_proc_imp.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll NetChannel_t* openChannelInnerClient(int socktype, const char* ip, unsigned short port, struct StackCoSche_t* sche);
__declspec_dll NetChannel_t* openListenerInner(int socktype, const char* ip, unsigned short port, struct StackCoSche_t* sche);

#ifdef __cplusplus
}
#endif

#endif // !BOOT_SERVER_CHANNEL_INNER_H
