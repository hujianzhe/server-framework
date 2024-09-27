#ifndef BOOT_SERVER_NET_CHANNEL_HIREDIS_H
#define	BOOT_SERVER_NET_CHANNEL_HIREDIS_H

#include "net_channel_proc_imp.h"
#include "util/inc/crt/protocol/hiredis_cli_protocol.h"

struct DispatchNetMsg_t;
typedef void(*FnChannelRedisOnSubscribe_t)(NetChannel_t*, struct DispatchNetMsg_t*, RedisReply_t*);

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll NetChannel_t* openNetChannelRedisClient(const char* ip, unsigned short port, FnChannelRedisOnSubscribe_t on_subscribe, void* sche);
__declspec_dll void sendRedisCmdByNetChannel(NetChannel_t* channel, int64_t rpc_id, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
