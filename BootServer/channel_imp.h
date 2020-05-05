#ifndef	CHANNEL_IMP_H
#define	CHANNEL_IMP_H

#include "util/inc/component/reactor.h"
#include "util/inc/component/channel.h"

enum {
	CHANNEL_TYPE_INNER = 1,
	CHANNEL_TYPE_HTTP
};

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll Channel_t* openChannel(ReactorObject_t* o, int flag, const void* saddr);
__declspec_dll ReactorObject_t* openListener(int domain, int socktype, const char* ip, unsigned short port);

__declspec_dll Channel_t* openChannelHttp(ReactorObject_t* o, int flag, const void* saddr);
__declspec_dll ReactorObject_t* openListenerHttp(int domain, const char* ip, unsigned short port);

#ifdef __cplusplus
}
#endif

#endif
