#include "config.h"
#include "global.h"
#include "channel_imp.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

static int defaultOnHeartbeat(Channel_t* c, int heartbeat_times) {
	if (heartbeat_times < c->heartbeat_maxtimes) {
		InnerMsg_t msg;
		makeInnerMsgEmpty(&msg);
		channelSendv(c, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
		return 1;
	}
	return 0;
}

static void defaultOnSynAck(ChannelBase_t* c, long long ts_msec) {
	Channel_t* channel = pod_container_of(c, Channel_t, _);
	channelEnableHeartbeat(channel, ts_msec);
}

void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec) {
	Channel_t* channel = pod_container_of(c, Channel_t, _);
	ChannelUserData_t* ud = (ChannelUserData_t*)channel->userdata;
	if (1 != c->connected_times) {
		return;
	}
	channelEnableHeartbeat(channel, ts_msec);
	if (ud->rpc_syn_ack_item) {
		UserMsg_t* msg = newUserMsg(0);
		msg->channel = channel;
		msg->rpcid = ud->rpc_syn_ack_item->id;
		msg->rpc_status = RPC_STATUS_RESP;
		dataqueuePush(&g_TaskThread->dq, &msg->internal._);
	}
}

static void channel_reg_handler(ChannelBase_t* c, long long timestamp_msec) {
	Channel_t* channel;
	unsigned short channel_flag;
	IPString_t ip = { 0 };
	unsigned short port = 0;
	const char* socktype_str;
	if (!sockaddrDecode(&c->to_addr.sa, ip, &port)) {
		logErr(&g_Log, "%s sockaddr decode error, ip:%s port:%hu", __FUNCTION__, ip, port);
		return;
	}

	channel = pod_container_of(c, Channel_t, _);
	channel_flag = channel->_.flag;
	socktype_str = (channel_flag & CHANNEL_FLAG_STREAM) ? "tcp" : "udp";
	if (channel_flag & CHANNEL_FLAG_CLIENT) {
		logInfo(&g_Log, "%s connect addr %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
		channelSendv(channel, NULL, 0, NETPACKET_SYN);
	}
	else if (channel_flag & CHANNEL_FLAG_LISTEN) {
		logInfo(&g_Log, "%s listen addr %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
	}
	else if (channel_flag & CHANNEL_FLAG_SERVER) {
		logInfo(&g_Log, "%s server reg %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
		channelEnableHeartbeat(channel, timestamp_msec);
	}
}

static void channel_detach(ChannelBase_t* channel) {
	dataqueuePush(&g_TaskThread->dq, &channel->freecmd._);
}

/*************************************************************************/
static const unsigned int CHANNEL_BASEHDRSIZE = 4;
static const unsigned int CHANNEL_EXTHDRSIZE = 5;
static unsigned int lengthfieldframe_hdrsize(Channel_t* c, unsigned int bodylen) {
	return CHANNEL_BASEHDRSIZE + CHANNEL_EXTHDRSIZE;
}

static void innerchannel_decode(Channel_t* c, unsigned char* buf, size_t buflen, ChannelInbufDecodeResult_t* decode_result) {
	unsigned char* data;
	unsigned int datalen;
	int res = lengthfieldframeDecode(CHANNEL_BASEHDRSIZE, buf, buflen, &data, &datalen);
	if (res < 0) {
		decode_result->err = 1;
	}
	else if (0 == res) {
		decode_result->incomplete = 1;
	}
	else {
		if (datalen < CHANNEL_EXTHDRSIZE) {
			decode_result->err = 1;
			return;
		}
		decode_result->pktype = *data;
		decode_result->pkseq = ntohl(*(unsigned int*)(data + 1));
		data += CHANNEL_EXTHDRSIZE;
		datalen -= CHANNEL_EXTHDRSIZE;

		decode_result->bodyptr = data;
		decode_result->bodylen = datalen;
		decode_result->decodelen = res;
	}
}

static void innerchannel_encode(Channel_t* c, const ChannelOutbufEncodeParam_t* param) {
	*(param->buf + CHANNEL_BASEHDRSIZE) = param->pktype;
	*(unsigned int*)(param->buf + CHANNEL_BASEHDRSIZE + 1) = htonl(param->pkseq);
	lengthfieldframeEncode(param->buf, CHANNEL_BASEHDRSIZE, param->bodylen + CHANNEL_EXTHDRSIZE);
}

static void innerchannel_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, long long ts_msec) {
	Channel_t* listen_channel = pod_container_of(listen_c, Channel_t, _);
	ReactorObject_t* listen_o = listen_c->o;
	IPString_t ip;
	unsigned short port;
	ReactorObject_t* o = reactorobjectOpen(newfd, listen_o->domain, listen_o->socktype, listen_o->protocol);
	if (!o) {
		socketClose(newfd);
		return;
	}
	listen_channel = openChannelInner(o, CHANNEL_FLAG_SERVER, peer_addr);
	if (!listen_channel) {
		reactorCommitCmd(NULL, &o->freecmd);
		return;
	}
	reactorCommitCmd(selectReactor(), &o->regcmd);
	if (sockaddrDecode(peer_addr, ip, &port))
		logInfo(&g_Log, "accept new socket(%p), ip:%s, port:%hu", o, ip, port);
	else
		logErr(&g_Log, "accept parse sockaddr error");
}

static void innerchannel_reply_ack(Channel_t* c, unsigned int seq, const struct sockaddr* addr) {
	unsigned int hdrsize = c->on_hdrsize(c, 0);
	unsigned char* buf = (unsigned char*)alloca(hdrsize);
	ChannelOutbufEncodeParam_t encode_param;
	encode_param.bodylen = 0;
	encode_param.hdrlen = hdrsize;
	encode_param.pkseq = seq;
	encode_param.pktype = NETPACKET_ACK;
	encode_param.buf = buf;
	c->on_encode(c, &encode_param);
	socketWrite(c->_.o->fd, buf, hdrsize, 0, addr, sockaddrLength(addr));
}

static void innerchannel_recv(Channel_t* c, const struct sockaddr* addr, ChannelInbufDecodeResult_t* decode_result) {
	unsigned int cmdid_rpcid_sz = 9;
	if (decode_result->bodylen >= cmdid_rpcid_sz) {
		UserMsg_t* message = newUserMsg(decode_result->bodylen - cmdid_rpcid_sz);
		if (!message) {
			return;
		}
		message->be_from_cluster = 1;
		message->channel = c;
		if (!(c->_.flag & CHANNEL_FLAG_STREAM)) {
			memcpy(&message->peer_addr, addr, sockaddrLength(addr));
		}
		message->rpc_status = *(decode_result->bodyptr);
		message->retcode = message->cmdid = ntohl(*(int*)(decode_result->bodyptr + 1));
		message->rpcid = ntohl(*(int*)(decode_result->bodyptr + 5));
		if (message->datalen) {
			memcpy(message->data, decode_result->bodyptr + cmdid_rpcid_sz, message->datalen);
		}
		if (g_Config.enqueue_timeout_msec > 0) {
			message->enqueue_time_msec = gmtimeMillisecond();
		}
		dataqueuePush(&g_TaskThread->dq, &message->internal._);
	}
	else if (c->_.flag & CHANNEL_FLAG_SERVER) {
		InnerMsg_t packet;
		makeInnerMsgEmpty(&packet);
		channelSendv(c, packet.iov, sizeof(packet.iov) / sizeof(packet.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
	}
}

/**************************************************************************/

Channel_t* openChannelInner(ReactorObject_t* o, int flag, const struct sockaddr* addr) {
	ChannelUserData_t* ud;
	Channel_t* c = reactorobjectOpenChannel(o, flag, sizeof(ChannelUserData_t), addr);
	if (!c)
		return NULL;
	//
	ud = (ChannelUserData_t*)(c + 1);
	ud->session = NULL;
	ud->rpc_syn_ack_item = NULL;
	ud->ws_handshake_state = 0;
	//
	c->userdata = ud;
	// c->_.write_fragment_size = 500;
	c->_.on_reg = channel_reg_handler;
	c->_.on_detach = channel_detach;
	c->on_hdrsize = lengthfieldframe_hdrsize;
	c->on_decode = innerchannel_decode;
	c->on_encode = innerchannel_encode;
	c->dgram.on_reply_ack = innerchannel_reply_ack;
	c->on_recv = innerchannel_recv;
	flag = c->_.flag;
	if (flag & CHANNEL_FLAG_CLIENT) {
		c->heartbeat_timeout_sec = 10;
		c->heartbeat_maxtimes = 3;
		c->on_heartbeat = defaultOnHeartbeat;
		c->_.on_syn_ack = defaultOnSynAck;
	}
	else if (flag & CHANNEL_FLAG_SERVER)
		c->heartbeat_timeout_sec = 20;
	if (flag & CHANNEL_FLAG_STREAM) {
		if ((flag & CHANNEL_FLAG_CLIENT) || (flag & CHANNEL_FLAG_SERVER)) {
			int on = g_Config.tcp_nodelay;
			setsockopt(o->fd, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));
		}
	}
	else {
		c->_.dgram_ctx.cwndsize = g_Config.udp_cwndsize;
	}
	return c;
}

Channel_t* openListenerInner(int socktype, const char* ip, unsigned short port) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	Channel_t* c;
	int domain = ipstrFamily(ip);
	if (!sockaddrEncode(&local_saddr.sa, domain, ip, port))
		return NULL;
	o = reactorobjectOpen(INVALID_FD_HANDLE, domain, socktype, 0);
	if (!o)
		return NULL;
	if (!socketBindAddr(o->fd, &local_saddr.sa, sockaddrLength(&local_saddr.sa))) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	if (SOCK_STREAM == socktype) {
		if (!socketTcpListen(o->fd)) {
			reactorCommitCmd(NULL, &o->freecmd);
			return NULL;
		}
	}
	c = openChannelInner(o, CHANNEL_FLAG_LISTEN, &local_saddr.sa);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	c->_.on_ack_halfconn = innerchannel_accept_callback;
	c->_.on_detach = channel_detach;
	return c;
}

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
	message->param.type = USER_MSG_EXTRA_HTTP_FRAME;
	message->param.httpframe = httpframe;
	message->cmdstr = httpframe->uri;
	message->rpc_status = 0;
	message->cmdid = 0;
	message->rpcid = 0;
	if (message->datalen) {
		memcpy(message->data, decode_result->bodyptr, message->datalen);
	}
	if (g_Config.enqueue_timeout_msec > 0) {
		message->enqueue_time_msec = gmtimeMillisecond();
	}
	dataqueuePush(&g_TaskThread->dq, &message->internal._);
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
	conn_channel = openChannelHttp(o, CHANNEL_FLAG_SERVER, peer_addr);
	if (!conn_channel) {
		reactorCommitCmd(NULL, &o->freecmd);
		return;
	}
	conn_channel->on_recv = listen_channel->on_recv;

	reactorCommitCmd(selectReactor(), &o->regcmd);
	if (sockaddrDecode(peer_addr, ip, &port))
		logInfo(&g_Log, "accept new socket(%p), ip:%s, port:%hu", o, ip, port);
	else
		logErr(&g_Log, "accept parse sockaddr error");
}

Channel_t* openChannelHttp(ReactorObject_t* o, int flag, const struct sockaddr* addr) {
	ChannelUserData_t* ud;
	Channel_t* c = reactorobjectOpenChannel(o, flag, sizeof(ChannelUserData_t), addr);
	if (!c)
		return NULL;
	//
	ud = (ChannelUserData_t*)(c + 1);
	ud->session = NULL;
	ud->rpc_syn_ack_item = NULL;
	ud->ws_handshake_state = 0;
	//
	c->userdata = ud;
	// c->_.write_fragment_size = 500;
	c->_.on_reg = channel_reg_handler;
	c->_.on_detach = channel_detach;
	c->on_decode = httpframe_decode;
	c->on_recv = httpframe_recv;
	flag = c->_.flag;
	if (flag & CHANNEL_FLAG_SERVER)
		c->heartbeat_timeout_sec = 20;
	return c;
}

Channel_t* openListenerHttp(const char* ip, unsigned short port, FnChannelOnRecv_t fn) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	Channel_t* c;
	int domain = ipstrFamily(ip);
	if (!sockaddrEncode(&local_saddr.sa, domain, ip, port))
		return NULL;
	o = reactorobjectOpen(INVALID_FD_HANDLE, domain, SOCK_STREAM, 0);
	if (!o)
		return NULL;
	if (!socketBindAddr(o->fd, &local_saddr.sa, sockaddrLength(&local_saddr.sa))) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	if (!socketTcpListen(o->fd)) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	c = reactorobjectOpenChannel(o, CHANNEL_FLAG_LISTEN, 0, &local_saddr.sa);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	c->_.on_reg = channel_reg_handler;
	c->_.on_ack_halfconn = http_accept_callback;
	c->_.on_detach = channel_detach;
	c->on_recv = fn ? fn : httpframe_recv;
	return c;
}

/**************************************************************************/

static unsigned int websocket_hdrsize(Channel_t* c, unsigned int bodylen) {
	ChannelUserData_t* ud = (ChannelUserData_t*)c->userdata;
	if (ud->ws_handshake_state > 1)
		return websocketframeEncodeHeadLength(bodylen);
	else
		return 0;
}

static void websocket_encode(Channel_t* c, const ChannelOutbufEncodeParam_t* param) {
	ChannelUserData_t* ud = (ChannelUserData_t*)c->userdata;
	if (ud->ws_handshake_state > 1)
		websocketframeEncode(param->buf, 1, WEBSOCKET_BINARY_FRAME, param->bodylen);
	else
		ud->ws_handshake_state = 2;
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
	conn_channel = openChannelWebsocketServer(o, peer_addr);
	if (!conn_channel) {
		reactorCommitCmd(NULL, &o->freecmd);
		return;
	}
	conn_channel->on_recv = listen_channel->on_recv;

	reactorCommitCmd(selectReactor(), &o->regcmd);
	if (sockaddrDecode(peer_addr, ip, &port))
		logInfo(&g_Log, "accept new socket(%p), ip:%s, port:%hu", o, ip, port);
	else
		logErr(&g_Log, "accept parse sockaddr error");
}

Channel_t* openChannelWebsocketServer(ReactorObject_t* o, const struct sockaddr* addr) {
	ChannelUserData_t* ud;
	Channel_t* c = reactorobjectOpenChannel(o, CHANNEL_FLAG_SERVER, sizeof(ChannelUserData_t), addr);
	if (!c)
		return NULL;
	//
	ud = (ChannelUserData_t*)(c + 1);
	ud->session = NULL;
	ud->rpc_syn_ack_item = NULL;
	ud->ws_handshake_state = 0;
	//
	c->userdata = ud;
	// c->_.write_fragment_size = 500;
	c->_.on_reg = channel_reg_handler;
	c->_.on_detach = channel_detach;
	c->on_hdrsize = websocket_hdrsize;
	c->on_decode = websocket_decode;
	c->on_encode = websocket_encode;
	c->on_recv = websocket_recv;
	c->heartbeat_timeout_sec = 20;
	return c;
}

Channel_t* openListenerWebsocket(const char* ip, unsigned short port, FnChannelOnRecv_t fn) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	Channel_t* c;
	int domain = ipstrFamily(ip);
	if (!sockaddrEncode(&local_saddr.sa, domain, ip, port))
		return NULL;
	o = reactorobjectOpen(INVALID_FD_HANDLE, domain, SOCK_STREAM, 0);
	if (!o)
		return NULL;
	if (!socketBindAddr(o->fd, &local_saddr.sa, sockaddrLength(&local_saddr.sa))) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	if (!socketTcpListen(o->fd)) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	c = reactorobjectOpenChannel(o, CHANNEL_FLAG_LISTEN, 0, &local_saddr.sa);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	c->_.on_reg = channel_reg_handler;
	c->_.on_ack_halfconn = websocket_accept_callback;
	c->_.on_detach = channel_detach;
	c->on_recv = fn ? fn : websocket_recv;
	return c;
}

#ifdef __cplusplus
}
#endif
