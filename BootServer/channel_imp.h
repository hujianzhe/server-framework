#ifndef	CHANNEL_IMP_H
#define	CHANNEL_IMP_H

#include "util/inc/component/reactor.h"
#include "util/inc/component/channel.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int defaultOnHeartbeat(Channel_t* c, int heartbeat_times);
__declspec_dllexport void defaultOnSynAck(ChannelBase_t* c, long long ts_msec);
__declspec_dllexport void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec);

__declspec_dllexport Channel_t* openChannel(ReactorObject_t* o, int flag, const void* saddr);
__declspec_dllexport ReactorObject_t* openListener(int domain, int socktype, const char* ip, unsigned short port);

__declspec_dllexport Channel_t* openChannelHttp(ReactorObject_t* o, int flag, const void* saddr);
__declspec_dllexport ReactorObject_t* openListenerHttp(int domain, const char* ip, unsigned short port);

#ifdef __cplusplus
}
#endif

#endif
