#ifndef	BOOT_SERVER_CHANNEL_PROC_IMP_H
#define	BOOT_SERVER_CHANNEL_PROC_IMP_H

#include "util/inc/component/net_reactor.h"
#include "util/inc/component/net_channel_ex.h"

struct StackCoSche_t;

typedef struct ChannelUserData_t {
	struct StackCoSche_t* sche;
	int rpc_id_syn_ack;
	int text_data_print_log;
} ChannelUserData_t;

typedef void(*FnChannelOnRecv_t)(NetChannel_t*, unsigned char*, size_t, const struct sockaddr*, socklen_t);

#define	channelUserData(channel)	((ChannelUserData_t*)((channel)->userdata))
#define	channelSetUserData(channel, ud)	((channel)->userdata = (ud))

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll void fnNetDispatchStackCoSche(struct StackCoSche_t* sche, StackCoAsyncParam_t* param);

__declspec_dll ChannelUserData_t* initChannelUserData(ChannelUserData_t* ud, struct StackCoSche_t* sche);
__declspec_dll void defaultRpcOnSynAck(NetChannel_t* c, long long ts_msec);
__declspec_dll void defaultChannelOnDetach(NetChannel_t* c);

#ifdef __cplusplus
}
#endif

#endif
