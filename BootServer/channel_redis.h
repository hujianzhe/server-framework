#ifndef BOOT_SERVER_CHANNEL_HIREDIS_H
#define	BOOT_SERVER_CHANNEL_HIREDIS_H

#include "channel_proc_imp.h"
#include "util/inc/crt/protocol/hiredis_cli_protocol.h"

struct DispatchNetMsg_t;
typedef void(*FnChannelRedisOnSubscribe_t)(ChannelBase_t*, struct DispatchNetMsg_t*, RedisReply_t*);

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll ChannelBase_t* openChannelRedisClient(const char* ip, unsigned short port, FnChannelRedisOnSubscribe_t on_subscribe, struct StackCoSche_t* sche);
__declspec_dll void channelRedisClientAsyncSendCommand(ChannelBase_t* channel, int rpc_id, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
