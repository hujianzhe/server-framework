#ifndef	CHANNEL_IMP_H
#define	CHANNEL_IMP_H

#include "util/inc/component/reactor.h"
#include "util/inc/component/channel.h"

struct Session_t;
struct UserMsg_t;
typedef struct ChannelUserData_t {
	int session_id;
	struct Session_t* session;
	List_t rpc_itemlist;
	int ws_handshake_state;
} ChannelUserData_t;

typedef void(*FnChannelOnRecv_t)(Channel_t*, const void*, ChannelInbufDecodeResult_t*);

#define	channelSession(channel)		(((ChannelUserData_t*)((channel)->userdata))->session)
#define	channelSessionId(channel)	(((ChannelUserData_t*)((channel)->userdata))->session_id)

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec);

__declspec_dllexport Channel_t* openChannelInner(ReactorObject_t* o, int flag, const void* saddr);
__declspec_dllexport ReactorObject_t* openListenerInner(int socktype, const char* ip, unsigned short port);

__declspec_dllexport Channel_t* openChannelHttp(ReactorObject_t* o, int flag, const void* saddr);
__declspec_dllexport ReactorObject_t* openListenerHttp(const char* ip, unsigned short port, FnChannelOnRecv_t fn);

__declspec_dllexport Channel_t* openChannelWebsocketServer(ReactorObject_t* o, const void* saddr);
__declspec_dllexport ReactorObject_t* openListenerWebsocket(const char* ip, unsigned short port, FnChannelOnRecv_t fn);

#ifdef __cplusplus
}
#endif

#endif
