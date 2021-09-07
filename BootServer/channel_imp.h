#ifndef	BOOT_SERVER_CHANNEL_IMP_H
#define	BOOT_SERVER_CHANNEL_IMP_H

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

__declspec_dll void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec);

__declspec_dll Channel_t* openChannelInner(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq);
__declspec_dll Channel_t* openListenerInner(int socktype, const char* ip, unsigned short port, struct DataQueue_t* dq);

__declspec_dll Channel_t* openChannelHttp(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq);
__declspec_dll Channel_t* openListenerHttp(const char* ip, unsigned short port, FnChannelOnRecv_t fn, struct DataQueue_t* dq);

__declspec_dll Channel_t* openChannelWebsocketServer(ReactorObject_t* o, const struct sockaddr* addr, struct DataQueue_t* dq);
__declspec_dll Channel_t* openListenerWebsocket(const char* ip, unsigned short port, FnChannelOnRecv_t fn, struct DataQueue_t* dq);

#ifdef __cplusplus
}
#endif

#endif
