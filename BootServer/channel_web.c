#include "global.h"
#include "channel_web.h"

typedef struct ChannelUserDataHttp_t {
	ChannelUserData_t _;
	int rpc_id_recv;
} ChannelUserDataHttp_t;

typedef struct ChannelUserDataWebsocket_t {
	ChannelUserData_t _;
	void(*on_recv)(ChannelBase_t* channel, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* addr);
	short ws_handshake_state;
	short ws_prev_is_fin;
} ChannelUserDataWebsocket_t;

static ChannelUserData_t* init_channel_user_data_http(ChannelUserDataHttp_t* ud, struct DataQueue_t* dq) {
	ud->rpc_id_recv = 0;
	return initChannelUserData(&ud->_, dq);
}

static ChannelUserData_t* init_channel_user_data_websocket(ChannelUserDataWebsocket_t* ud, struct DataQueue_t* dq) {
	ud->ws_handshake_state = 0;
	ud->ws_prev_is_fin = 1;
	return initChannelUserData(&ud->_, dq);
}

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/

static void free_user_msg(UserMsg_t* msg) {
	free(httpframeReset(msg->param.httpframe));
	free(msg);
}

static void httpframe_recv(ChannelBase_t* c, HttpFrame_t* httpframe, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* addr) {
	ChannelUserDataHttp_t* ud;
	UserMsg_t* message = newUserMsg(bodylen);
	if (!message) {
		free(httpframeReset(httpframe));
		return;
	}
	message->channel = c;
	if (!(c->flag & CHANNEL_FLAG_STREAM)) {
		memcpy(&message->peer_addr, addr, sockaddrLength(addr));
	}
	httpframe->uri[httpframe->pathlen] = 0;
	message->param.type = USER_MSG_PARAM_HTTP_FRAME;
	message->param.httpframe = httpframe;
	message->cmdstr = httpframe->uri;
	message->cmdid = 0;
	message->on_free = free_user_msg;

	ud = (ChannelUserDataHttp_t*)channelUserData(c);
	if (ud->rpc_id_recv != 0) {
		message->rpc_status = RPC_STATUS_RESP;
		message->rpcid = ud->rpc_id_recv;
		ud->rpc_id_recv = 0;
	}
	else {
		message->rpc_status = 0;
		message->rpcid = 0;
	}
	if (message->datalen) {
		memcpy(message->data, bodyptr, message->datalen);
	}
	if (ptrBSG()->conf->enqueue_timeout_msec > 0) {
		message->enqueue_time_msec = gmtimeMillisecond();
	}
	dataqueuePush(channelUserData(c)->dq, &message->internal._);
}

static int httpframe_on_read(ChannelBase_t* c, unsigned char* buf, unsigned int buflen, long long timestamp_msec, const struct sockaddr* addr) {
	int res;
	HttpFrame_t* frame = (HttpFrame_t*)malloc(sizeof(HttpFrame_t));
	if (!frame) {
		return -1;
	}
	res = httpframeDecodeHeader(frame, (char*)buf, buflen);
	if (res < 0) {
		free(frame);
		return -1;
	}
	if (0 == res) {
		free(frame);
		return 0;
	}
	if (frame->content_length) {
		if (frame->content_length > buflen - res) {
			free(httpframeReset(frame));
			return 0;
		}
		if (frame->multipart_form_data_boundary &&
			frame->multipart_form_data_boundary[0])
		{
			if (!httpframeDecodeMultipartFormDataList(frame, buf + res, frame->content_length)) {
				free(httpframeReset(frame));
				return -1;
			}
			httpframe_recv(c, frame, NULL, 0, addr);
		}
		else {
			httpframe_recv(c, frame, buf + res, frame->content_length, addr);
		}
	}
	else {
		httpframe_recv(c, frame, NULL, 0, addr);
	}
	return res + frame->content_length;
}

static void http_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, long long ts_msec) {
	ChannelBase_t* conn_channel;
	ReactorObject_t* listen_o = listen_c->o;
	IPString_t ip;
	unsigned short port;
	ReactorObject_t* o = reactorobjectOpen(newfd, listen_o->domain, listen_o->socktype, listen_o->protocol);
	if (!o) {
		socketClose(newfd);
		return;
	}
	conn_channel = openChannelHttp(o, CHANNEL_FLAG_SERVER, peer_addr, channelUserData(listen_c)->dq);
	if (!conn_channel) {
		reactorCommitCmd(NULL, &o->freecmd);
		return;
	}

	reactorCommitCmd(selectReactor(), &o->regcmd);
	if (sockaddrDecode(peer_addr, ip, &port)) {
		logInfo(ptrBSG()->log, "accept new socket(%p), ip:%s, port:%hu", o, ip, port);
	}
	else {
		logErr(ptrBSG()->log, "accept parse sockaddr error");
	}
}

static ChannelBaseProc_t s_http_proc = {
	defaultChannelOnReg,
	NULL,
	httpframe_on_read,
	NULL,
	NULL,
	NULL,
	defaultChannelOnDetach,
	NULL
};

ChannelBase_t* openChannelHttp(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq) {
	ChannelUserDataHttp_t* ud;
	ChannelBase_t* c = channelbaseOpen(sizeof(ChannelBase_t) + sizeof(ChannelUserDataHttp_t), flag, o, addr);
	if (!c) {
		return NULL;
	}
	//
	ud = (ChannelUserDataHttp_t*)(c + 1);
	channelSetUserData(c, init_channel_user_data_http(ud, dq));
	c->proc = &s_http_proc;
	flag = c->flag;
	if (flag & CHANNEL_FLAG_SERVER) {
		c->heartbeat_timeout_sec = 20;
	}
	return c;
}

ChannelBase_t* openListenerHttp(const char* ip, unsigned short port, struct DataQueue_t* dq) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	ChannelBase_t* c;
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
	c = openChannelHttp(o, CHANNEL_FLAG_LISTEN, &local_saddr.sa, dq);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	c->on_ack_halfconn = http_accept_callback;
	return c;
}

/**************************************************************************/

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
		else if (0 == res) {
			return 0;
		}
		else {
			if (WEBSOCKET_CLOSE_FRAME == type) {
				return -1;
			}
			ud->on_recv(c, data, datalen, addr);
			return res;
		}
	}
	else {
		char* key;
		unsigned int keylen;
		int res = websocketframeDecodeHandshake((char*)buf, buflen, &key, &keylen);
		if (res < 0) {
			return -1;
		}
		else if (0 == res) {
			return 0;
		}
		else {
			char txt[162];
			if (!websocketframeEncodeHandshake(key, keylen, txt)) {
				return -1;
			}
			channelbaseSend(c, txt, strlen(txt), NETPACKET_NO_ACK_FRAGMENT);
			ud->ws_handshake_state = 1;
			return res;
		}
	}
}

static void websocket_recv(ChannelBase_t* c, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* addr) {}
static ChannelBase_t* openChannelWebsocket(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq);
static void websocket_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, long long ts_msec) {
	ChannelUserDataWebsocket_t* listen_ud = (ChannelUserDataWebsocket_t*)channelUserData(listen_c);
	ChannelBase_t* conn_channel;
	ChannelUserDataWebsocket_t* conn_ud;
	ReactorObject_t* listen_o = listen_c->o;
	IPString_t ip;
	unsigned short port;
	ReactorObject_t* o = reactorobjectOpen(newfd, listen_o->domain, listen_o->socktype, listen_o->protocol);
	if (!o) {
		socketClose(newfd);
		return;
	}
	conn_channel = openChannelWebsocket(o, CHANNEL_FLAG_SERVER, peer_addr, channelUserData(listen_c)->dq);
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

static ChannelBaseProc_t s_websocket_proc = {
	defaultChannelOnReg,
	NULL,
	websocket_on_read,
	websocket_hdrsize,
	websocket_on_pre_send,
	NULL,
	defaultChannelOnDetach,
	NULL
};

static ChannelBase_t* openChannelWebsocket(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq) {
	ChannelUserDataWebsocket_t* ud;
	ChannelBase_t* c = channelbaseOpen(sizeof(ChannelBase_t) + sizeof(ChannelUserDataWebsocket_t), flag, o, addr);
	if (!c) {
		return NULL;
	}
	//
	ud = (ChannelUserDataWebsocket_t*)(c + 1);
	channelSetUserData(c, init_channel_user_data_websocket(ud, dq));
	// c->_.write_fragment_size = 500;
	ud->on_recv = websocket_recv;
	c->proc = &s_websocket_proc;
	flag = c->flag;
	if (flag & CHANNEL_FLAG_SERVER) {
		c->heartbeat_timeout_sec = 20;
	}
	if (flag & CHANNEL_FLAG_STREAM) {
		if ((flag & CHANNEL_FLAG_CLIENT) || (flag & CHANNEL_FLAG_SERVER)) {
			int on = ptrBSG()->conf->tcp_nodelay;
			setsockopt(o->fd, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));
		}
	}
	return c;
}

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
	c = openChannelWebsocket(o, CHANNEL_FLAG_LISTEN, &local_saddr.sa, dq);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	ud = (ChannelUserDataWebsocket_t*)channelUserData(c);
	c->on_ack_halfconn = websocket_accept_callback;
	ud->on_recv = fn ? fn : websocket_recv;
	return c;
}

#ifdef __cplusplus
}
#endif
