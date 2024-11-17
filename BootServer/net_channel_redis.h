#ifndef BOOT_SERVER_NET_CHANNEL_HIREDIS_H
#define	BOOT_SERVER_NET_CHANNEL_HIREDIS_H

#include "net_channel_proc_imp.h"
#include "util/inc/crt/protocol/hiredis_cli_protocol.h"

typedef void(*FnChannelRedisOnSubscribe_t)(NetChannel_t*, const char*, size_t, const unsigned char*, size_t, long long);

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll NetChannel_t* openNetChannelRedisClient(const BootServerConfigConnectOption_t* opt, FnChannelRedisOnSubscribe_t on_subscribe, void* sche);
__declspec_dll void sendRedisCmdByNetChannel(NetChannel_t* channel, int64_t rpc_id, const char* format, ...);
__declspec_dll void sendRedisFormatCmdByNetChannel(NetChannel_t* channel, int64_t rpc_id, const char* fmt_cmd, size_t fmt_cmd_len);

#ifdef __cplusplus
}
#endif

#endif
