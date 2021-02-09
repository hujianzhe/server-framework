#include "global.h"
#include "session_struct.h"

static Atom32_t CHANNEL_SESSION_ID = 0;

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
	session->channel_client = NULL;
	session->channel_server = NULL;
	session->id = 0;
	session->userdata = NULL;
	session->disconnect = NULL;
	session->destroy = NULL;
	session->expire_timeout_msec = 0;
	session->expire_timeout_ev = NULL;
	return session;
}

void sessionChannelReplaceClient(Session_t* session, Channel_t* channel) {
	Channel_t* old_channel = session->channel_client;
	if (old_channel == channel)
		return;
	if (old_channel) {
		channelSession(old_channel) = NULL;
		channelSessionId(old_channel) = 0;
		channelSendv(old_channel, NULL, 0, NETPACKET_FIN);
	}
	session->channel_client = channel;
	if (channel) {
		channelSession(channel) = session;
		channelSessionId(channel) = session->id;
	}
}

void sessionChannelReplaceServer(Session_t* session, Channel_t* channel) {
	Channel_t* old_channel = session->channel_server;
	if (old_channel == channel)
		return;
	if (old_channel) {
		channelSession(old_channel) = NULL;
		channelSessionId(old_channel) = 0;
		channelSendv(old_channel, NULL, 0, NETPACKET_FIN);
	}
	session->channel_server = channel;
	if (channel) {
		channelSession(channel) = session;
		channelSessionId(channel) = session->id;
	}
}

void sessionDisconnect(Session_t* session) {
	if (session->channel_client) {
		channelSendv(session->channel_client, NULL, 0, NETPACKET_FIN);
		channelSession(session->channel_client) = NULL;
		channelSessionId(session->channel_client) = 0;
		session->channel_client = NULL;
	}
	if (session->channel_server) {
		channelSendv(session->channel_server, NULL, 0, NETPACKET_FIN);
		channelSession(session->channel_server) = NULL;
		channelSessionId(session->channel_server) = 0;
		session->channel_server = NULL;
	}
}

void sessionUnbindChannel(Session_t* session) {
	if (session->channel_client) {
		channelSession(session->channel_client) = NULL;
		channelSessionId(session->channel_client) = 0;
		session->channel_client = NULL;
	}
	if (session->channel_server) {
		channelSession(session->channel_server) = NULL;
		channelSessionId(session->channel_server) = 0;
		session->channel_server = NULL;
	}
}

Channel_t* sessionChannel(Session_t* session) {
	if (session->channel_client)
		return session->channel_client;
	else if (session->channel_server)
		return session->channel_server;
	else
		return NULL;
}

#ifdef __cplusplus
}
#endif