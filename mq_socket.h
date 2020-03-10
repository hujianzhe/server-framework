#ifndef	MQ_SOCKET_H
#define	MQ_SOCKET_H

#include "util/inc/component/reactor.h"
#include "util/inc/component/channel.h"

Channel_t* mqsocketOpenChannel(ReactorObject_t* o, int flag, const void* saddr);
ReactorObject_t* mqlistenOpen(int domain, int socktype, const char* ip, unsigned short port);

#endif
