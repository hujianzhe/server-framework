#ifndef BOOT_SERVER_CHANNEL_HIREDIS_H
#define	BOOT_SERVER_CHANNEL_HIREDIS_H

#include "channel_proc_imp.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll ChannelBase_t* openChannelRedisClient(const char* ip, unsigned short port, struct DataQueue_t* dq);
__declspec_dll void channelRedisClientAsyncSendCommand(ChannelBase_t* channel, int rpc_id, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
