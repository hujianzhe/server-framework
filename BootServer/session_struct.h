#ifndef BOOT_SERVER_SESSION_STRUCT_H
#define	BOOT_SERVER_SESSION_STRUCT_H

#include "util/inc/component/reactor.h"
#include <time.h>

struct TaskThread_t;

typedef struct Session_t {
	int reconnect_delay_sec;
	time_t reconnect_timestamp_sec;
	ChannelBase_t* channel_client;
	ChannelBase_t* channel_server;
	char* id;
	void* userdata;
	int socktype;
	IPString_t ip;
	unsigned short port;
	/* interface */
	ChannelBase_t*(*do_connect_handshake)(struct TaskThread_t*, struct Session_t*); /* optional */
	void(*on_disconnect)(struct TaskThread_t*, struct Session_t*); /* optional */
	void(*destroy)(struct Session_t*); /* optional */
} Session_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll Session_t* initSession(Session_t* session);
__declspec_dll void sessionReplaceChannel(Session_t* session, ChannelBase_t* channel);
__declspec_dll void sessionDisconnect(Session_t* session);
__declspec_dll void sessionUnbindChannel(Session_t* session);
__declspec_dll ChannelBase_t* sessionChannel(Session_t* session);

#ifdef __cplusplus
}
#endif

#endif // !SESSION_STRUCT_H
