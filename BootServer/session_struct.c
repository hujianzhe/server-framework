#include "global.h"
#include "session_struct.h"

static HashtableNode_t* s_SessionBulk[1024];
static Atom32_t CHANNEL_SESSION_ID = 0;

static Session_t* defaultNewSession(int usertype) {
	Session_t* session = (Session_t*)malloc(sizeof(Session_t));
	if (session) {
		initSession(session);
		session->usertype = usertype;
	}
	return session;
}
static void defaultFreeSession(Session_t* s) { free(s); }
SessionActon_t g_SessionAction = { defaultNewSession, NULL, defaultFreeSession };

#ifdef __cplusplus
extern "C" {
#endif

int allocSessionId(void) {
	int session_id;
	do {
		session_id = _xadd32(&CHANNEL_SESSION_ID, 1) + 1;
	} while (0 == session_id);
	return session_id;
}

Session_t* initSession(Session_t* session) {
	session->has_reg = 0;
	session->persist = 0;
	session->id = 0;
	session->usertype = 0;
	session->userdata = NULL;
	listInit(&session->rpc_itemlist);
	session->expire_timeout_msec = 0;
	session->expire_timeout_ev = NULL;
	return session;
}

void sessionBindChannel(Session_t* session, Channel_t* channel) {
	session->channel = channel;
	channelSession(channel) = session;
	channelSessionId(channel) = session->id;
}

Channel_t* sessionUnbindChannel(Session_t* session) {
	if (session) {
		Channel_t* channel = session->channel;
		if (channel) {
			channelSession(channel) = NULL;
			channelSessionId(channel) = 0;
		}
		session->channel = NULL;
		return channel;
	}
	return NULL;
}

#ifdef __cplusplus
}
#endif