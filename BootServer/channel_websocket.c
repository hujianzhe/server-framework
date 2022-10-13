#include "global.h"
#include "channel_websocket.h"

typedef struct ChannelUserDataWebsocket_t {
	ChannelUserData_t _;
	void(*on_recv)(ChannelBase_t* channel, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* addr);
	DynArr_t(unsigned char) fragment_recv;
	short ws_handshake_state;
	short ws_prev_is_fin;
} ChannelUserDataWebsocket_t;

static ChannelUserData_t* init_channel_user_data_websocket(ChannelUserDataWebsocket_t* ud, struct DataQueue_t* dq) {
	dynarrInitZero(&ud->fragment_recv);
	ud->ws_handshake_state = 0;
	ud->ws_prev_is_fin = 1;
	return initChannelUserData(&ud->_, dq);
}

/********************************************************************/

static unsigned int websocket_hdrsize(ChannelBase_t* base, unsigned int bodylen) {
	ChannelUserDataWebsocket_t* ud = (ChannelUserDataWebsocket_t*)channelUserData(base);
	if (ud->ws_handshake_state > 1) {
		return websocketframeEncodeHeadLength(bodylen);
	}
	return 0;
}

static int websocket_on_pre_send(ChannelBase_t* c, NetPacket_t* packet, long long timestamp_msec) {
	ChannelUserDataWebsocket_t* ud = (ChannelUserDataWebsocket_t*)channelUserData(c);
	if (ud->ws_handshake_state > 1) {
		websocketframeEncode(packet->buf, packet->fragment_eof, ud->ws_prev_is_fin, WEBSOCKET_BINARY_FRAME, packet->bodylen);
		ud->ws_prev_is_fin = packet->fragment_eof;
	}
	else {
		ud->ws_handshake_state = 2;
	}
	return 1;
}

static int websocket_on_read(ChannelBase_t* c, unsigned char* buf, unsigned int buflen, long long timestamp_msec, const struct sockaddr* addr) {
	ChannelUserDataWebsocket_t* ud = (ChannelUserDataWebsocket_t*)channelUserData(c);
	if (ud->ws_handshake_state >= 1) {
		unsigned char* data;
		unsigned long long datalen;
		int is_fin, type;
		int res = websocketframeDecode(buf, buflen, &data, &datalen, &is_fin, &type);
		if (res < 0) {
			return -1;
		}
		if (0 == res) {
			return 0;
		}
		if (WEBSOCKET_CLOSE_FRAME == type) {
			return -1;
		}
		if (is_fin && dynarrIsEmpty(&ud->fragment_recv)) {
			ud->on_recv(c, data, datalen, addr);
		}
		else {
			int ok;
			dynarrCopyAppend(&ud->fragment_recv, data, datalen, ok);
			if (!ok) {
				return -1;
			}
			if (!is_fin) {
				return res;
			}
			ud->on_recv(c, ud->fragment_recv.buf, ud->fragment_recv.len, addr);
			dynarrClearData(&ud->fragment_recv);
		}
		return res;
	}
	else {
		const char* sec_key, *sec_protocol;
		unsigned int sec_keylen, sec_protocol_len, sec_accept_len;
		char sec_accept[60];
		int res = websocketframeDecodeHandshakeRequest((char*)buf, buflen, &sec_key, &sec_keylen, &sec_protocol, &sec_protocol_len);
		if (res < 0) {
			return -1;
		}
		if (0 == res) {
			return 0;
		}
		if (!websocketframeComputeSecAccept(sec_key, sec_keylen, sec_accept)) {
			return -1;
		}
		sec_accept_len = strlen(sec_accept);
		if (sec_protocol) {
			char* txt = websocketframeEncodeHandshakeResponseWithProtocol(sec_accept, sec_accept_len, sec_protocol, sec_protocol_len);
			if (!txt) {
				return -1;
			}
			channelbaseSend(c, txt, strlen(txt), NETPACKET_NO_ACK_FRAGMENT);
			utilExportFree(txt);
		}
		else {
			char txt[162];
			if (!websocketframeEncodeHandshakeResponse(sec_accept, sec_accept_len, txt)) {
				return -1;
			}
			channelbaseSend(c, txt, strlen(txt), NETPACKET_NO_ACK_FRAGMENT);
		}
		ud->ws_handshake_state = 1;
		return res;
	}
}

static void websocket_on_free(ChannelBase_t* channel) {
	ChannelUserDataWebsocket_t* ud = (ChannelUserDataWebsocket_t*)channelUserData(channel);
	dynarrFreeMemory(&ud->fragment_recv);
}

static ChannelBaseProc_t s_websocket_server_proc = {
	defaultChannelOnReg,
	NULL,
	websocket_on_read,
	websocket_hdrsize,
	websocket_on_pre_send,
	NULL,
	defaultChannelOnDetach,
	websocket_on_free
};

static ChannelBase_t* openChannelWebsocketServer(ReactorObject_t* o, const struct sockaddr* addr, struct DataQueue_t* dq) {
	ChannelUserDataWebsocket_t* ud;
	ChannelBase_t* c = channelbaseOpen(sizeof(ChannelBase_t) + sizeof(ChannelUserDataWebsocket_t), CHANNEL_FLAG_SERVER, o, addr);
	if (!c) {
		return NULL;
	}
	//
	ud = (ChannelUserDataWebsocket_t*)(c + 1);
	channelSetUserData(c, init_channel_user_data_websocket(ud, dq));
	// c->_.write_fragment_size = 500;
	c->proc = &s_websocket_server_proc;
	c->heartbeat_timeout_sec = 20;
	return c;
}

static void websocket_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, long long ts_msec) {
	ChannelUserDataWebsocket_t* listen_ud = (ChannelUserDataWebsocket_t*)channelUserData(listen_c);
	ChannelBase_t* conn_channel;
	ChannelUserDataWebsocket_t* conn_ud;
	ReactorObject_t* listen_o = listen_c->o;
	IPString_t ip;
	unsigned short port;
	int on;
	ReactorObject_t* o = reactorobjectOpen(newfd, listen_o->domain, listen_o->socktype, listen_o->protocol);
	if (!o) {
		socketClose(newfd);
		return;
	}
	on = ptrBSG()->conf->tcp_nodelay;
	setsockopt(o->fd, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));

	conn_channel = openChannelWebsocketServer(o, peer_addr, channelUserData(listen_c)->dq);
	if (!conn_channel) {
		reactorCommitCmd(NULL, &o->freecmd);
		return;
	}
	conn_ud = (ChannelUserDataWebsocket_t*)channelUserData(conn_channel);
	conn_ud->on_recv = listen_ud->on_recv;

	reactorCommitCmd(selectReactor(), &o->regcmd);

	if (sockaddrDecode(peer_addr, ip, &port)) {
		logInfo(ptrBSG()->log, "accept new socket(%p), ip:%s, port:%hu", o, ip, port);
	}
	else {
		logErr(ptrBSG()->log, "accept parse sockaddr error");
	}
}

#ifdef __cplusplus
extern "C" {
#endif

ChannelBase_t* openListenerWebsocket(const char* ip, unsigned short port, FnChannelOnRecv_t fn, struct DataQueue_t* dq) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	ChannelBase_t* c;
	ChannelUserDataWebsocket_t* ud;
	int domain = ipstrFamily(ip);
	if (!sockaddrEncode(&local_saddr.sa, domain, ip, port)) {
		return NULL;
	}
	o = reactorobjectOpen(INVALID_FD_HANDLE, domain, SOCK_STREAM, 0);
	if (!o) {
		return NULL;
	}
	if (!socketEnableReuseAddr(o->fd, TRUE)) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	if (bind(o->fd, &local_saddr.sa, sockaddrLength(&local_saddr.sa))) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	if (!socketTcpListen(o->fd)) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}

	c = channelbaseOpen(sizeof(ChannelBase_t) + sizeof(ChannelUserDataWebsocket_t), CHANNEL_FLAG_LISTEN, o, &local_saddr.sa);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	c->proc = &s_websocket_server_proc;
	c->on_ack_halfconn = websocket_accept_callback;
	ud = (ChannelUserDataWebsocket_t*)(c + 1);
	channelSetUserData(c, init_channel_user_data_websocket(ud, dq));
	ud->on_recv = fn;
	return c;
}

#ifdef __cplusplus
}
#endif
