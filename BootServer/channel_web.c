#include "global.h"
#include "channel_web.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/

static void httpframe_decode(Channel_t* c, unsigned char* buf, size_t buflen, ChannelInbufDecodeResult_t* decode_result) {
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
		decode_result->userdata = frame;
	}
}

static void httpframe_recv(Channel_t* c, const struct sockaddr* addr, ChannelInbufDecodeResult_t* decode_result) {
	ChannelUserData_t* ud;
	HttpFrame_t* httpframe = (HttpFrame_t*)decode_result->userdata;
	UserMsg_t* message = newUserMsg(decode_result->bodylen);
	if (!message) {
		free(httpframeReset(httpframe));
		return;
	}
	message->channel = c;
	if (!(c->_.flag & CHANNEL_FLAG_STREAM)) {
		memcpy(&message->peer_addr, addr, sockaddrLength(addr));
	}
	httpframe->uri[httpframe->pathlen] = 0;
	message->param.type = USER_MSG_PARAM_HTTP_FRAME;
	message->param.httpframe = httpframe;
	message->cmdstr = httpframe->uri;
	message->cmdid = 0;

	ud = (ChannelUserData_t*)c->userdata;
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
	Channel_t* listen_channel = pod_container_of(listen_c, Channel_t, _);
	Channel_t* conn_channel;
	ReactorObject_t* listen_o = listen_c->o;
	IPString_t ip;
	unsigned short port;
	ReactorObject_t* o = reactorobjectOpen(newfd, listen_o->domain, listen_o->socktype, listen_o->protocol);
	if (!o) {
		socketClose(newfd);
		return;
	}
	conn_channel = openChannelHttp(o, CHANNEL_FLAG_SERVER, peer_addr, channelUserData(listen_channel)->dq);
	if (!conn_channel) {
		reactorCommitCmd(NULL, &o->freecmd);
		return;
	}
	conn_channel->on_recv = listen_channel->on_recv;

	reactorCommitCmd(selectReactor(), &o->regcmd);
	if (sockaddrDecode(peer_addr, ip, &port)) {
		logInfo(ptrBSG()->log, "accept new socket(%p), ip:%s, port:%hu", o, ip, port);
	}
	else {
		logErr(ptrBSG()->log, "accept parse sockaddr error");
	}
}

Channel_t* openChannelHttp(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq) {
	ChannelUserData_t* ud;
	Channel_t* c = reactorobjectOpenChannel(o, flag, sizeof(ChannelUserData_t), addr);
	if (!c) {
		return NULL;
	}
	//
	ud = (ChannelUserData_t*)(c + 1);
	c->userdata = initChannelUserData(ud, dq);
	// c->_.write_fragment_size = 500;
	c->_.on_reg = defaultChannelOnReg;
	c->_.on_detach = defaultChannelOnDetach;
	c->on_decode = httpframe_decode;
	c->on_recv = httpframe_recv;
	flag = c->_.flag;
	if (flag & CHANNEL_FLAG_SERVER) {
		c->_.heartbeat_timeout_sec = 20;
	}
	return c;
}

Channel_t* openListenerHttp(const char* ip, unsigned short port, FnChannelOnRecv_t fn, struct DataQueue_t* dq) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	Channel_t* c;
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
	c->_.on_ack_halfconn = http_accept_callback;
	c->on_recv = fn ? fn : httpframe_recv;
	return c;
}

/**************************************************************************/

static unsigned int websocket_hdrsize(ChannelBase_t* base, unsigned int bodylen) {
	Channel_t* c = pod_container_of(base, Channel_t, _);
	ChannelUserData_t* ud = (ChannelUserData_t*)c->userdata;
	if (ud->ws_handshake_state > 1) {
		return websocketframeEncodeHeadLength(bodylen);
	}
	else {
		return 0;
	}
}

static void websocket_encode(Channel_t* c, const ChannelOutbufEncodeParam_t* param) {
	ChannelUserData_t* ud = (ChannelUserData_t*)c->userdata;
	if (ud->ws_handshake_state > 1) {
		websocketframeEncode(param->buf, param->fragment_eof, ud->ws_prev_is_fin, WEBSOCKET_BINARY_FRAME, param->bodylen);
		ud->ws_prev_is_fin = param->fragment_eof;
	}
	else {
		ud->ws_handshake_state = 2;
	}
}

static void websocket_decode(Channel_t* c, unsigned char* buf, size_t buflen, ChannelInbufDecodeResult_t* decode_result) {
	ChannelUserData_t* ud = (ChannelUserData_t*)c->userdata;
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
			channelSend(c, txt, strlen(txt), NETPACKET_NO_ACK_FRAGMENT);
			ud->ws_handshake_state = 1;
		}
	}
}

static void websocket_recv(Channel_t* c, const struct sockaddr* addr, ChannelInbufDecodeResult_t* decode_result) {}

static void websocket_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, long long ts_msec) {
	Channel_t* listen_channel = pod_container_of(listen_c, Channel_t, _);
	Channel_t* conn_channel;
	ReactorObject_t* listen_o = listen_c->o;
	IPString_t ip;
	unsigned short port;
	ReactorObject_t* o = reactorobjectOpen(newfd, listen_o->domain, listen_o->socktype, listen_o->protocol);
	if (!o) {
		socketClose(newfd);
		return;
	}
	conn_channel = openChannelWebsocketServer(o, peer_addr, channelUserData(listen_channel)->dq);
	if (!conn_channel) {
		reactorCommitCmd(NULL, &o->freecmd);
		return;
	}
	conn_channel->on_recv = listen_channel->on_recv;

	reactorCommitCmd(selectReactor(), &o->regcmd);
	if (sockaddrDecode(peer_addr, ip, &port)) {
		logInfo(ptrBSG()->log, "accept new socket(%p), ip:%s, port:%hu", o, ip, port);
	}
	else {
		logErr(ptrBSG()->log, "accept parse sockaddr error");
	}
}

static Channel_t* openChannelWebsocket(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq) {
	ChannelUserData_t* ud;
	Channel_t* c = reactorobjectOpenChannel(o, flag, sizeof(ChannelUserData_t), addr);
	if (!c) {
		return NULL;
	}
	//
	ud = (ChannelUserData_t*)(c + 1);
	c->userdata = initChannelUserData(ud, dq);
	// c->_.write_fragment_size = 500;
	c->_.on_reg = defaultChannelOnReg;
	c->_.on_detach = defaultChannelOnDetach;
	c->on_recv = websocket_recv;
	flag = c->_.flag;
	if (flag & CHANNEL_FLAG_SERVER) {
		c->_.heartbeat_timeout_sec = 20;
	}
	if (flag & CHANNEL_FLAG_STREAM) {
		if ((flag & CHANNEL_FLAG_CLIENT) || (flag & CHANNEL_FLAG_SERVER)) {
			int on = ptrBSG()->conf->tcp_nodelay;
			setsockopt(o->fd, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));
		}
	}
	return c;
}

Channel_t* openChannelWebsocketServer(ReactorObject_t* o, const struct sockaddr* addr, struct DataQueue_t* dq) {
	Channel_t* c = openChannelWebsocket(o, CHANNEL_FLAG_SERVER, addr, dq);
	c->_.on_hdrsize = websocket_hdrsize;
	c->on_decode = websocket_decode;
	c->on_encode = websocket_encode;
	return c;
}

Channel_t* openListenerWebsocket(const char* ip, unsigned short port, FnChannelOnRecv_t fn, struct DataQueue_t* dq) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	Channel_t* c;
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
	c->_.on_ack_halfconn = websocket_accept_callback;
	c->on_recv = fn ? fn : websocket_recv;
	return c;
}

#ifdef __cplusplus
}
#endif
