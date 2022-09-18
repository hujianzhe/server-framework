#ifndef	BOOT_SERVER_CHANNEL_PROC_IMP_H
#define	BOOT_SERVER_CHANNEL_PROC_IMP_H

#include "util/inc/component/reactor.h"
#include "util/inc/component/channel.h"

struct Session_t;
struct DataQueue_t;
typedef struct ChannelUserData_t {
	struct Session_t* session;
	struct DataQueue_t* dq;
	int rpc_id_syn_ack;
	int text_data_print_log;
} ChannelUserData_t;

typedef void(*FnChannelOnRecv_t)(Channel_t*, const struct sockaddr*, ChannelInbufDecodeResult_t*);

#define	channelUserData(channel)	((ChannelUserData_t*)((channel)->_.userdata))
#define	channelSetUserData(channel, ud)	((channel)->_.userdata = (ud))
#define	channelSession(channel)		(channelUserData(channel)->session)

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll ChannelUserData_t* initChannelUserData(ChannelUserData_t* ud, struct DataQueue_t* dq);
__declspec_dll void defaultChannelOnReg(ChannelBase_t* c, long long timestamp_msec);
__declspec_dll void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec);
__declspec_dll void defaultChannelOnDetach(ChannelBase_t* c);

#ifdef __cplusplus
}
#endif

#endif
