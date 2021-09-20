#ifndef	BOOT_SERVER_CHANNEL_PROC_IMP_H
#define	BOOT_SERVER_CHANNEL_PROC_IMP_H

#include "util/inc/component/reactor.h"
#include "util/inc/component/channel.h"

struct RpcItem_t;
struct Session_t;
struct DataQueue_t;
typedef struct ChannelUserData_t {
	int session_id;
	struct Session_t* session;
	struct RpcItem_t* rpc_syn_ack_item;
	struct DataQueue_t* dq;
	int ws_handshake_state;
	int text_data_print_log;
} ChannelUserData_t;

typedef void(*FnChannelOnRecv_t)(Channel_t*, const struct sockaddr*, ChannelInbufDecodeResult_t*);

#define	channelUserData(channel)	((ChannelUserData_t*)((channel)->userdata))
#define	channelSession(channel)		(((ChannelUserData_t*)((channel)->userdata))->session)
#define	channelSessionId(channel)	(((ChannelUserData_t*)((channel)->userdata))->session_id)

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll ChannelUserData_t* initChannelUserDtata(ChannelUserData_t* ud, struct DataQueue_t* dq);
__declspec_dll void defaultChannelOnReg(ChannelBase_t* c, long long timestamp_msec);
__declspec_dll void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec);
__declspec_dll void defaultChannelOnDetach(ChannelBase_t* c);

#ifdef __cplusplus
}
#endif

#endif
