#ifndef BOOT_SERVER_SESSION_STRUCT_H
#define	BOOT_SERVER_SESSION_STRUCT_H

#include "util/inc/component/channel.h"
#include <time.h>

typedef struct Session_t {
	short has_reg;
	int reconnect_delay_sec;
	time_t reconnect_timestamp_sec;
	Channel_t* channel_client;
	Channel_t* channel_server;
	char* id;
	void* userdata;
	void(*disconnect)(struct Session_t*);
	void(*destroy)(struct Session_t*);
} Session_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll int allocSessionId(void);
__declspec_dll Session_t* initSession(Session_t* session);
__declspec_dll void sessionReplaceChannel(Session_t* session, Channel_t* channel);
__declspec_dll void sessionDisconnect(Session_t* session);
__declspec_dll void sessionUnbindChannel(Session_t* session);
__declspec_dll Channel_t* sessionChannel(Session_t* session);

#ifdef __cplusplus
}
#endif

#endif // !SESSION_STRUCT_H
