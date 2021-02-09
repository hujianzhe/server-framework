#ifndef SESSION_STRUCT_H
#define	SESSION_STRUCT_H

#include "util/inc/component/channel.h"

typedef struct Session_t {
	short has_reg;
	short persist;
	Channel_t* channel_client;
	Channel_t* channel_server;
	int id;
	void* userdata;
	void(*disconnect)(struct Session_t*);
	void(*destroy)(struct Session_t*);
	unsigned int expire_timeout_msec;
	RBTimerEvent_t* expire_timeout_ev;
} Session_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int allocSessionId(void);
__declspec_dllexport Session_t* initSession(Session_t* session);

void sessionChannelReplaceClient(Session_t* session, Channel_t* channel);
void sessionChannelReplaceServer(Session_t* session, Channel_t* channel);
__declspec_dllexport void sessionDisconnect(Session_t* session);
__declspec_dllexport void sessionUnbindChannel(Session_t* session);
__declspec_dllexport Channel_t* sessionChannel(Session_t* session);

#ifdef __cplusplus
}
#endif

#endif // !SESSION_STRUCT_H
