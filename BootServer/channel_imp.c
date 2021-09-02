#include "config.h"
#include "global.h"
#include "channel_imp.h"
#include "task_thread.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

static ChannelUserData_t* init_channel_user_data(ChannelUserData_t* ud) {
	ud->session = NULL;
	ud->rpc_syn_ack_item = NULL;
	ud->dq = NULL;
	ud->ws_handshake_state = 0;
	return ud;
}

static int defaultOnHeartbeat(ChannelBase_t* c, int heartbeat_times) {
	if (heartbeat_times < c->heartbeat_maxtimes) {
		Channel_t* channel = pod_container_of(c, Channel_t, _);
		InnerMsg_t msg;
		makeInnerMsgEmpty(&msg);
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
		return 1;
	}
	return 0;
}

void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec) {
	Channel_t* channel = pod_container_of(c, Channel_t, _);
	ChannelUserData_t* ud = (ChannelUserData_t*)channel->userdata;
	if (1 != c->connected_times) {
		return;
	}
	if (ud->rpc_syn_ack_item) {
		UserMsg_t* msg = newUserMsg(0);
		msg->channel = channel;
		msg->rpcid = ud->rpc_syn_ack_item->id;
		msg->rpc_status = RPC_STATUS_RESP;
		dataqueuePush(ud->dq, &msg->internal._);
		ud->rpc_syn_ack_item = NULL;
	}
}

static void channel_reg_handler(ChannelBase_t* c, long long timestamp_msec) {
	Channel_t* channel;
	unsigned short channel_flag;
	IPString_t ip = { 0 };
	unsigned short port = 0;
	const char* socktype_str;
	if (!sockaddrDecode(&c->to_addr.sa, ip, &port)) {
		logErr(ptrBSG()->log, "%s sockaddr decode error, ip:%s port:%hu", __FUNCTION__, ip, port);
		return;
	}

	channel = pod_container_of(c, Channel_t, _);
	channel_flag = channel->_.flag;
	socktype_str = (channel_flag & CHANNEL_FLAG_STREAM) ? "tcp" : "udp";
	if (channel_flag & CHANNEL_FLAG_CLIENT) {
		logInfo(ptrBSG()->log, "%s connect addr %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
		channelSendv(channel, NULL, 0, NETPACKET_SYN);
	}
	else if (channel_flag & CHANNEL_FLAG_LISTEN) {
		logInfo(ptrBSG()->log, "%s listen addr %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
	}
	else if (channel_flag & CHANNEL_FLAG_SERVER) {
		logInfo(ptrBSG()->log, "%s server reg %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
	}
}

static void channel_detach(ChannelBase_t* c) {
	Channel_t* channel = pod_container_of(c, Channel_t, _);
	dataqueuePush(channelUserData(channel)->dq, &c->freecmd._);
}

/*************************************************************************/
#define	INNER_BASEHDRSIZE 4
#define INNER_EXTHDRSIZE 6
#define	INNER_HDRSIZE 10
static unsigned int innerchannel_hdrsize(Channel_t* c, unsigned int bodylen) { return INNER_HDRSIZE; }

static void innerchannel_decode(Channel_t* c, unsigned char* buf, size_t buflen, ChannelInbufDecodeResult_t* decode_result) {
	unsigned char* data;
	unsigned int datalen;
	int res = lengthfieldframeDecode(INNER_BASEHDRSIZE, buf, buflen, &data, &datalen);
	if (res < 0) {
		decode_result->err = 1;
	}
	else if (0 == res) {
		decode_result->incomplete = 1;
	}
	else {
		if (datalen < INNER_EXTHDRSIZE) {
			decode_result->err = 1;
			return;
		}
		decode_result->pktype = data[0];
		decode_result->fragment_eof = data[1];
		decode_result->pkseq = ntohl(*(unsigned int*)&data[2]);
		data += INNER_EXTHDRSIZE;
		datalen -= INNER_EXTHDRSIZE;

		decode_result->bodyptr = data;
		decode_result->bodylen = datalen;
		decode_result->decodelen = res;
	}
}

static void innerchannel_encode(Channel_t* c, const ChannelOutbufEncodeParam_t* param) {
	unsigned char* exthdr = param->buf + INNER_BASEHDRSIZE;
	exthdr[0] = param->pktype;
	exthdr[1] = param->fragment_eof;
	*(unsigned int*)&exthdr[2] = htonl(param->pkseq);
	lengthfieldframeEncode(param->buf, INNER_BASEHDRSIZE, param->bodylen + INNER_EXTHDRSIZE);
}

static void innerchannel_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, long long ts_msec) {
	Channel_t* listen_channel = pod_container_of(listen_c, Channel_t, _);
	ReactorObject_t* listen_o = listen_c->o;
	Channel_t* conn_channel;
	IPString_t ip;
	unsigned short port;
	ReactorObject_t* o = reactorobjectOpen(newfd, listen_o->domain, listen_o->socktype, listen_o->protocol);
	if (!o) {
		socketClose(newfd);
		return;
	}
	conn_channel = openChannelInner(o, CHANNEL_FLAG_SERVER, peer_addr, channelUserData(listen_channel)->dq);
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

static void innerchannel_reply_ack(Channel_t* c, unsigned int seq, const struct sockaddr* addr) {
	unsigned char buf[INNER_HDRSIZE];
	ChannelOutbufEncodeParam_t encode_param;
	encode_param.bodylen = 0;
	encode_param.hdrlen = sizeof(buf);
	encode_param.pkseq = seq;
	encode_param.fragment_eof = 1;
	encode_param.pktype = NETPACKET_ACK;
	encode_param.buf = buf;
	c->on_encode(c, &encode_param);
	socketWrite(c->_.o->fd, buf, sizeof(buf), 0, addr, sockaddrLength(addr));
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
		if (ptrBSG()->conf->enqueue_timeout_msec > 0) {
			message->enqueue_time_msec = gmtimeMillisecond();
		}
		dataqueuePush(channelUserData(c)->dq, &message->internal._);
	}
	else if (c->_.flag & CHANNEL_FLAG_SERVER) {
		InnerMsg_t packet;
		makeInnerMsgEmpty(&packet);
		channelSendv(c, packet.iov, sizeof(packet.iov) / sizeof(packet.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
	}
}

/**************************************************************************/

Channel_t* openChannelInner(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq) {
	ChannelUserData_t* ud;
	Channel_t* c = reactorobjectOpenChannel(o, flag, sizeof(ChannelUserData_t), addr);
	if (!c) {
		return NULL;
	}
	//
	ud = (ChannelUserData_t*)(c + 1);
	init_channel_user_data(ud);
	ud->dq = dq;
	c->userdata = ud;
	// c->_.write_fragment_size = 500;
	c->_.on_reg = channel_reg_handler;
	c->_.on_detach = channel_detach;
	c->on_hdrsize = innerchannel_hdrsize;
	c->on_decode = innerchannel_decode;
	c->on_encode = innerchannel_encode;
	c->dgram.on_reply_ack = innerchannel_reply_ack;
	c->on_recv = innerchannel_recv;
	flag = c->_.flag;
	if (flag & CHANNEL_FLAG_CLIENT) {
		c->_.heartbeat_timeout_sec = 10;
		c->_.heartbeat_maxtimes = 3;
		c->_.on_heartbeat = defaultOnHeartbeat;
	}
	else if (flag & CHANNEL_FLAG_SERVER) {
		c->_.heartbeat_timeout_sec = 20;
	}
	if (flag & CHANNEL_FLAG_STREAM) {
		if ((flag & CHANNEL_FLAG_CLIENT) || (flag & CHANNEL_FLAG_SERVER)) {
			int on = ptrBSG()->conf->tcp_nodelay;
			setsockopt(o->fd, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));
		}
	}
	else {
		c->_.dgram_ctx.cwndsize = ptrBSG()->conf->udp_cwndsize;
	}
	return c;
}

Channel_t* openListenerInner(int socktype, const char* ip, unsigned short port, struct DataQueue_t* dq) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	Channel_t* c;
	int domain = ipstrFamily(ip);
	if (!sockaddrEncode(&local_saddr.sa, domain, ip, port)) {
		return NULL;
	}
	o = reactorobjectOpen(INVALID_FD_HANDLE, domain, socktype, 0);
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
	if (SOCK_STREAM == socktype) {
		if (!socketTcpListen(o->fd)) {
			reactorCommitCmd(NULL, &o->freecmd);
			return NULL;
		}
	}
	c = openChannelInner(o, CHANNEL_FLAG_LISTEN, &local_saddr.sa, dq);
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
	init_channel_user_data(ud);
	ud->dq = dq;
	c->userdata = ud;
	// c->_.write_fragment_size = 500;
	c->_.on_reg = channel_reg_handler;
	c->_.on_detach = channel_detach;
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

static unsigned int websocket_hdrsize(Channel_t* c, unsigned int bodylen) {
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
		websocketframeEncode(param->buf, 1, WEBSOCKET_BINARY_FRAME, param->bodylen);
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
	init_channel_user_data(ud);
	ud->dq = dq;
	c->userdata = ud;
	// c->_.write_fragment_size = 500;
	c->_.on_reg = channel_reg_handler;
	c->_.on_detach = channel_detach;
	c->on_recv = websocket_recv;
	if (flag & CHANNEL_FLAG_SERVER) {
		c->_.heartbeat_timeout_sec = 20;
	}
	return c;
}

Channel_t* openChannelWebsocketServer(ReactorObject_t* o, const struct sockaddr* addr, struct DataQueue_t* dq) {
	Channel_t* c = openChannelWebsocket(o, CHANNEL_FLAG_SERVER, addr, dq);
	c->on_hdrsize = websocket_hdrsize;
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
