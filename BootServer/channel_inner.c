#include "global.h"
#include "channel_inner.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	INNER_BASEHDRSIZE 4
#define INNER_EXTHDRSIZE 6
#define	INNER_HDRSIZE 10
static unsigned int innerchannel_hdrsize(ChannelBase_t* c, unsigned int bodylen) { return INNER_HDRSIZE; }

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
	ReactorObject_t* o = c->_.o;
	unsigned char buf[INNER_HDRSIZE];
	ChannelOutbufEncodeParam_t encode_param;
	encode_param.bodylen = 0;
	encode_param.hdrlen = sizeof(buf);
	encode_param.pkseq = seq;
	encode_param.fragment_eof = 1;
	encode_param.pktype = NETPACKET_ACK;
	encode_param.buf = buf;
	c->on_encode(c, &encode_param);
	if (o->m_connected) {
		addr = NULL;
	}
	sendto(o->fd, (char*)buf, sizeof(buf), 0, addr, sockaddrLength(addr));
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

static int innerchannel_heartbeat(ChannelBase_t* c, int heartbeat_times) {
	if (heartbeat_times < c->heartbeat_maxtimes) {
		Channel_t* channel = pod_container_of(c, Channel_t, _);
		InnerMsg_t msg;
		makeInnerMsgEmpty(&msg);
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
		return 1;
	}
	return 0;
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
	c->userdata = initChannelUserData(ud, dq);
	// c->_.write_fragment_size = 500;
	c->_.on_reg = defaultChannelOnReg;
	c->_.on_detach = defaultChannelOnDetach;
	c->_.on_hdrsize = innerchannel_hdrsize;
	c->on_decode = innerchannel_decode;
	c->on_encode = innerchannel_encode;
	c->dgram.on_reply_ack = innerchannel_reply_ack;
	c->on_recv = innerchannel_recv;
	flag = c->_.flag;
	if (flag & CHANNEL_FLAG_CLIENT) {
		c->_.heartbeat_timeout_sec = 10;
		c->_.heartbeat_maxtimes = 3;
		c->_.on_heartbeat = innerchannel_heartbeat;
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
	if (!socketEnableReusePort(o->fd, TRUE)) {
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
	c->_.on_detach = defaultChannelOnDetach;
	return c;
}

#ifdef __cplusplus
}
#endif
