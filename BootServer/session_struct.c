#include "global.h"
#include "session_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

Session_t* initSession(Session_t* session) {
	session->has_reg = 0;
	session->reconnect_delay_sec = 0;
	session->reconnect_timestamp_sec = 0;
	session->channel_client = NULL;
	session->channel_server = NULL;
	session->id = NULL;
	session->userdata = NULL;
	session->socktype = AF_UNSPEC;
	session->ip[0] = 0;
	session->port = 0;
	session->do_connect_handshake = NULL;
	session->on_disconnect = NULL;
	session->destroy = NULL;
	return session;
}

void sessionReplaceChannel(Session_t* session, ChannelBase_t* channel) {
	ChannelBase_t* old_channel;
	if (channel->flag & CHANNEL_FLAG_CLIENT) {
		old_channel = session->channel_client;
		if (old_channel == channel) {
			return;
		}
		session->channel_client = channel;
	}
	else if (channel->flag & CHANNEL_FLAG_SERVER) {
		old_channel = session->channel_server;
		if (old_channel == channel) {
			return;
		}
		session->channel_server = channel;
	}
	else {
		return;
	}
	if (old_channel) {
		channelSession(old_channel) = NULL;
		channelbaseSendv(old_channel, NULL, 0, NETPACKET_FIN);
	}
	if (channel) {
		channelSession(channel) = session;
	}
}

void sessionDisconnect(Session_t* session) {
	if (session->channel_client) {
		channelbaseSendv(session->channel_client, NULL, 0, NETPACKET_FIN);
		channelSession(session->channel_client) = NULL;
		session->channel_client = NULL;
	}
	if (session->channel_server) {
		channelbaseSendv(session->channel_server, NULL, 0, NETPACKET_FIN);
		channelSession(session->channel_server) = NULL;
		session->channel_server = NULL;
	}
}

void sessionUnbindChannel(Session_t* session) {
	if (session->channel_client) {
		channelSession(session->channel_client) = NULL;
		session->channel_client = NULL;
	}
	if (session->channel_server) {
		channelSession(session->channel_server) = NULL;
		session->channel_server = NULL;
	}
}

ChannelBase_t* sessionChannel(Session_t* session) {
	if (session->channel_client) {
		return session->channel_client;
	}
	else if (session->channel_server) {
		return session->channel_server;
	}
	else {
		return NULL;
	}
}

#ifdef __cplusplus
}
#endif
