#include "global.h"
#include "channel_web.h"

typedef struct ChannelUserDataHttp_t {
	ChannelUserData_t _;
	ChannelRWData_t rw;
	int rpc_id_recv;
	HttpFrame_t* frame;
} ChannelUserDataHttp_t;

typedef struct ChannelUserDataWebsocket_t {
	ChannelUserData_t _;
	ChannelRWData_t rw;
	short ws_handshake_state;
	short ws_prev_is_fin;
} ChannelUserDataWebsocket_t;

static ChannelUserData_t* init_channel_user_data_http(ChannelUserDataHttp_t* ud, ChannelBase_t* channel, struct DataQueue_t* dq) {
	channelrwdataInit(&ud->rw, channel->flag);
	channelbaseUseRWData(channel, &ud->rw);
	ud->rpc_id_recv = 0;
	return initChannelUserData(&ud->_, dq);
}

static ChannelUserData_t* init_channel_user_data_websocket(ChannelUserDataWebsocket_t* ud, ChannelBase_t* channel, struct DataQueue_t* dq) {
	channelrwdataInit(&ud->rw, channel->flag);
	channelbaseUseRWData(channel, &ud->rw);
	ud->ws_handshake_state = 0;
	ud->ws_prev_is_fin = 1;
	return initChannelUserData(&ud->_, dq);
}

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/

static void httpframe_decode(ChannelBase_t* c, unsigned char* buf, size_t buflen, ChannelInbufDecodeResult_t* decode_result) {
	int res;
	HttpFrame_t* frame = (HttpFrame_t*)malloc(sizeof(HttpFrame_t));
	if (!frame) {
		decode_result->err = 1;
		return;
	}
	res = httpframeDecodeHeader(frame, (char*)buf, buflen);
	if (res < 0) {
		decode_result->err = 1;
		free(frame);
	}
	else if (0 == res) {
		decode_result->incomplete = 1;
		free(frame);
	}
	else {
		if (frame->content_length) {
			if (frame->content_length > buflen - res) {
				decode_result->incomplete = 1;
				free(httpframeReset(frame));
				return;
			}
			if (frame->multipart_form_data_boundary &&
				frame->multipart_form_data_boundary[0])
			{
				decode_result->bodyptr = NULL;
				decode_result->bodylen = 0;
				if (!httpframeDecodeMultipartFormDataList(frame, buf + res, frame->content_length)) {
					decode_result->err = 1;
					free(httpframeReset(frame));
					return;
				}
			}
			else {
				decode_result->bodyptr = buf + res;
				decode_result->bodylen = frame->content_length;
			}
		}
		else {
			decode_result->bodyptr = NULL;
			decode_result->bodylen = 0;
		}
		decode_result->decodelen = res + frame->content_length;
		((ChannelUserDataHttp_t*)channelUserData(c))->frame = frame;
	}
}

static void free_user_msg(UserMsg_t* msg) {
	free(httpframeReset(msg->param.httpframe));
	free(msg);
}

static void httpframe_recv(ChannelBase_t* c, const struct sockaddr* addr, const ChannelInbufDecodeResult_t* decode_result) {
	ChannelUserDataHttp_t* ud = (ChannelUserDataHttp_t*)channelUserData(c);
	HttpFrame_t* httpframe = ud->frame;
	UserMsg_t* message;

	ud->frame = NULL;
	message = newUserMsg(decode_result->bodylen);
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
		memcpy(message->data, decode_result->bodyptr, message->datalen);
	}
	if (ptrBSG()->conf->enqueue_timeout_msec > 0) {
		message->enqueue_time_msec = gmtimeMillisecond();
	}
	dataqueuePush(channelUserData(c)->dq, &message->internal._);
}

static void http_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, long long ts_msec) {
	ChannelUserDataHttp_t* listen_ud = (ChannelUserDataHttp_t*)channelUserData(listen_c);
	ChannelBase_t* conn_channel;
	ChannelUserDataHttp_t* conn_ud;
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
	conn_ud = (ChannelUserDataHttp_t*)channelUserData(conn_channel);
	conn_ud->rw.on_recv = listen_ud->rw.on_recv;

	reactorCommitCmd(selectReactor(), &o->regcmd);
	if (sockaddrDecode(peer_addr, ip, &port)) {
		logInfo(ptrBSG()->log, "accept new socket(%p), ip:%s, port:%hu", o, ip, port);
	}
	else {
		logErr(ptrBSG()->log, "accept parse sockaddr error");
	}
}

ChannelBase_t* openChannelHttp(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq) {
	ChannelUserDataHttp_t* ud;
	ChannelBase_t* c = channelbaseOpen(sizeof(ChannelBase_t) + sizeof(ChannelUserDataHttp_t), flag, o, addr);
	if (!c) {
		return NULL;
	}
	//
	ud = (ChannelUserDataHttp_t*)(c + 1);
	channelSetUserData(c, init_channel_user_data_http(ud, c, dq));
	ud->rw.base_proc.on_reg = defaultChannelOnReg;
	ud->rw.base_proc.on_detach = defaultChannelOnDetach;
	ud->rw.on_decode = httpframe_decode;
	ud->rw.on_recv = httpframe_recv;
	flag = c->flag;
	if (flag & CHANNEL_FLAG_SERVER) {
		c->heartbeat_timeout_sec = 20;
	}
	return c;
}

ChannelBase_t* openListenerHttp(const char* ip, unsigned short port, FnChannelOnRecv_t fn, struct DataQueue_t* dq) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	ChannelBase_t* c;
	ChannelUserDataHttp_t* ud;
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
	ud = (ChannelUserDataHttp_t*)channelUserData(c);
	c->on_ack_halfconn = http_accept_callback;
	ud->rw.on_recv = fn ? fn : httpframe_recv;
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

static void websocket_encode(ChannelBase_t* c, NetPacket_t* packet) {
	ChannelUserDataWebsocket_t* ud = (ChannelUserDataWebsocket_t*)channelUserData(c);
	if (ud->ws_handshake_state > 1) {
		websocketframeEncode(packet->buf, packet->fragment_eof, ud->ws_prev_is_fin, WEBSOCKET_BINARY_FRAME, packet->bodylen);
		ud->ws_prev_is_fin = packet->fragment_eof;
	}
	else {
		ud->ws_handshake_state = 2;
	}
}

static void websocket_decode(ChannelBase_t* c, unsigned char* buf, size_t buflen, ChannelInbufDecodeResult_t* decode_result) {
	ChannelUserDataWebsocket_t* ud = (ChannelUserDataWebsocket_t*)channelUserData(c);
	if (ud->ws_handshake_state >= 1) {
		unsigned char* data;
		unsigned long long datalen;
		int is_fin, type;
		int res = websocketframeDecode(buf, buflen, &data, &datalen, &is_fin, &type);
		if (res < 0) {
			decode_result->err = 1;
		}
		else if (0 == res) {
			decode_result->incomplete = 1;
		}
		else {
			decode_result->decodelen = res;
			if (WEBSOCKET_CLOSE_FRAME == type) {
				decode_result->err = 1;
				return;
			}
			decode_result->bodyptr = data;
			decode_result->bodylen = datalen;
		}
	}
	else {
		char* key;
		unsigned int keylen;
		int res = websocketframeDecodeHandshake((char*)buf, buflen, &key, &keylen);
		if (res < 0) {
			decode_result->err = 1;
		}
		else if (0 == res) {
			decode_result->incomplete = 1;
		}
		else {
			char txt[162];
			if (!websocketframeEncodeHandshake(key, keylen, txt)) {
				decode_result->err = 1;
				return;
			}
			decode_result->ignore = 1;
			decode_result->decodelen = res;
			channelbaseSend(c, txt, strlen(txt), NETPACKET_NO_ACK_FRAGMENT);
			ud->ws_handshake_state = 1;
		}
	}
}

static void websocket_recv(ChannelBase_t* c, const struct sockaddr* addr, const ChannelInbufDecodeResult_t* decode_result) {}

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
	conn_channel = openChannelWebsocketServer(o, peer_addr, channelUserData(listen_c)->dq);
	if (!conn_channel) {
		reactorCommitCmd(NULL, &o->freecmd);
		return;
	}
	conn_ud = (ChannelUserDataWebsocket_t*)channelUserData(conn_channel);
	conn_ud->rw.on_recv = listen_ud->rw.on_recv;

	reactorCommitCmd(selectReactor(), &o->regcmd);
	if (sockaddrDecode(peer_addr, ip, &port)) {
		logInfo(ptrBSG()->log, "accept new socket(%p), ip:%s, port:%hu", o, ip, port);
	}
	else {
		logErr(ptrBSG()->log, "accept parse sockaddr error");
	}
}

static ChannelBase_t* openChannelWebsocket(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq) {
	ChannelUserDataWebsocket_t* ud;
	ChannelBase_t* c = channelbaseOpen(sizeof(ChannelBase_t) + sizeof(ChannelUserDataWebsocket_t), flag, o, addr);
	if (!c) {
		return NULL;
	}
	//
	ud = (ChannelUserDataWebsocket_t*)(c + 1);
	channelSetUserData(c, init_channel_user_data_websocket(ud, c, dq));
	// c->_.write_fragment_size = 500;
	ud->rw.base_proc.on_reg = defaultChannelOnReg;
	ud->rw.base_proc.on_detach = defaultChannelOnDetach;
	ud->rw.on_recv = websocket_recv;
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

ChannelBase_t* openChannelWebsocketServer(ReactorObject_t* o, const struct sockaddr* addr, struct DataQueue_t* dq) {
	ChannelBase_t* c = openChannelWebsocket(o, CHANNEL_FLAG_SERVER, addr, dq);
	ChannelUserDataWebsocket_t* ud = (ChannelUserDataWebsocket_t*)channelUserData(c);
	ud->rw.base_proc.on_hdrsize = websocket_hdrsize;
	ud->rw.on_decode = websocket_decode;
	ud->rw.on_encode = websocket_encode;
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
	ud->rw.on_recv = fn ? fn : websocket_recv;
	return c;
}

#ifdef __cplusplus
}
#endif
