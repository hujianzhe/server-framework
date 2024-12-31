#include "global.h"
#include "net_channel_websocket.h"

typedef struct NetChannelUserDataWebsocket_t {
	NetChannelUserData_t _;
	void(*on_recv)(NetChannel_t* channel, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* addr, socklen_t addrlen);
	DynArr_t(unsigned char) fragment_recv;
	short ws_handshake_state;
	short ws_prev_is_fin;
} NetChannelUserDataWebsocket_t;

static NetChannelUserData_t* init_channel_user_data_websocket(NetChannelUserDataWebsocket_t* ud, const BootServerConfigNetChannelOption_t* channel_opt, void* sche) {
	dynarrInitZero(&ud->fragment_recv);
	ud->ws_handshake_state = 0;
	ud->ws_prev_is_fin = 1;
	return initNetChannelUserData(&ud->_, channel_opt, sche);
}

/********************************************************************/

static unsigned int websocket_hdrsize(NetChannel_t* base, unsigned int bodylen) {
	NetChannelUserDataWebsocket_t* ud = (NetChannelUserDataWebsocket_t*)NetChannel_get_userdata(base);
	if (ud->ws_handshake_state > 1) {
		return websocketframeEncodeHeadLength(bodylen);
	}
	return 0;
}

static int websocket_on_pre_send(NetChannel_t* c, NetPacket_t* packet, long long timestamp_msec) {
	NetChannelUserDataWebsocket_t* ud = (NetChannelUserDataWebsocket_t*)NetChannel_get_userdata(c);
	if (ud->ws_handshake_state > 1) {
		websocketframeEncode(packet->buf, packet->fragment_eof, ud->ws_prev_is_fin, WEBSOCKET_BINARY_FRAME, packet->bodylen);
		ud->ws_prev_is_fin = packet->fragment_eof;
	}
	else {
		ud->ws_handshake_state = 2;
	}
	return 1;
}

static int websocket_on_read(NetChannel_t* c, unsigned char* buf, unsigned int buflen, long long timestamp_msec, const struct sockaddr* addr, socklen_t addrlen) {
	NetChannelUserDataWebsocket_t* ud = (NetChannelUserDataWebsocket_t*)NetChannel_get_userdata(c);
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
			ud->on_recv(c, data, datalen, addr, addrlen);
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
			ud->on_recv(c, ud->fragment_recv.buf, ud->fragment_recv.len, addr, addrlen);
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
			NetChannel_send(c, txt, strlen(txt), NETPACKET_NO_ACK_FRAGMENT, NULL, 0);
			websocketframeFreeString(txt);
		}
		else {
			char txt[162];
			if (!websocketframeEncodeHandshakeResponse(sec_accept, sec_accept_len, txt)) {
				return -1;
			}
			NetChannel_send(c, txt, strlen(txt), NETPACKET_NO_ACK_FRAGMENT, NULL, 0);
		}
		ud->ws_handshake_state = 1;
		return res;
	}
}

static void websocket_on_free(NetChannel_t* channel) {
	NetChannelUserDataWebsocket_t* ud = (NetChannelUserDataWebsocket_t*)NetChannel_get_userdata(channel);
	dynarrFreeMemory(&ud->fragment_recv);
	free(ud);
}

static NetChannelProc_t s_websocket_server_proc = {
	NULL,
	websocket_on_read,
	websocket_hdrsize,
	websocket_on_pre_send,
	NULL,
	defaultNetChannelOnDetach,
	websocket_on_free
};

static void websocket_accept_callback(NetChannel_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, socklen_t addrlen, long long ts_msec) {
	NetChannel_t* c = NULL;
	NetChannelUserDataWebsocket_t* conn_ud = NULL;
	const BootServerConfigNetChannelOption_t* channel_opt;
	NetChannelUserDataWebsocket_t* listen_ud;

	c = NetChannel_open_with_fd(NET_CHANNEL_SIDE_SERVER, &s_websocket_server_proc, newfd, peer_addr->sa_family, 0);
	if (!c) {
		socketClose(newfd);
		goto err;
	}
	conn_ud = (NetChannelUserDataWebsocket_t*)malloc(sizeof(NetChannelUserDataWebsocket_t));
	if (!conn_ud) {
		goto err;
	}
	listen_ud = (NetChannelUserDataWebsocket_t*)NetChannel_get_userdata(listen_c);
	channel_opt = &listen_ud->_.channel_opt;
	init_channel_user_data_websocket(conn_ud, channel_opt, listen_ud->_.sche);
	conn_ud->on_recv = listen_ud->on_recv;
	NetChannel_set_userdata(c, conn_ud);
	c->heartbeat_timeout_msec = channel_opt->heartbeat_timeout_msec > 0 ? channel_opt->heartbeat_timeout_msec : 20000;
	NetChannel_reg(selectNetReactor(), c);
	NetChannel_close_ref(c);
	return;
err:
	free(conn_ud);
	NetChannel_close_ref(c);
}

#ifdef __cplusplus
extern "C" {
#endif

NetChannel_t* openNetListenerWebsocket(const BootServerConfigListenOption_t* opt, FnNetChannelOnRecv_t fn, void* sche) {
	Sockaddr_t listen_saddr;
	socklen_t listen_saddrlen;
	NetChannel_t* c = NULL;
	NetChannelUserDataWebsocket_t* ud = NULL;
	const BootServerConfigNetChannelOption_t* channel_conf_opt = &opt->channel_opt;
	int domain = ipstrFamily(channel_conf_opt->ip);

	listen_saddrlen = sockaddrEncode(&listen_saddr.sa, domain, channel_conf_opt->ip, channel_conf_opt->port);
	if (listen_saddrlen <= 0) {
		return NULL;
	}
	ud = (NetChannelUserDataWebsocket_t*)malloc(sizeof(NetChannelUserDataWebsocket_t));
	if (!ud) {
		goto err;
	}
	c = NetChannel_open(NET_CHANNEL_SIDE_LISTEN, &s_websocket_server_proc, domain, SOCK_STREAM, 0);
	if (!c) {
		goto err;
	}
	c->listen_backlog = opt->backlog;
	if (!NetChannel_set_operator_sockaddr(c, &listen_saddr.sa, listen_saddrlen)) {
		goto err;
	}
	init_channel_user_data_websocket(ud, channel_conf_opt, sche);
	NetChannel_set_userdata(c, ud);
	ud->on_recv = fn;
	c->on_ack_halfconn = websocket_accept_callback;
	return c;
err:
	free(ud);
	NetChannel_close_ref(c);
	return NULL;
}

#ifdef __cplusplus
}
#endif
