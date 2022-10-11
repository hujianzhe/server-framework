#include "global.h"
#include "channel_inner.h"

typedef struct ChannelUserDataInner_t {
	ChannelUserData_t _;
	ChannelRWData_t rw;
} ChannelUserDataInner_t;

#ifdef __cplusplus
extern "C" {
#endif

#define	INNER_BASEHDRSIZE 4
#define INNER_EXTHDRSIZE 6
#define	INNER_HDRSIZE 10
static unsigned int innerchannel_hdrsize(ChannelBase_t* c, unsigned int bodylen) { return INNER_HDRSIZE; }

static void innerchannel_decode(ChannelBase_t* c, unsigned char* buf, size_t buflen, ChannelInbufDecodeResult_t* decode_result) {
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

static void innerchannel_encode(ChannelBase_t* c, NetPacket_t* packet) {
	unsigned char* exthdr = packet->buf + INNER_BASEHDRSIZE;
	exthdr[0] = packet->type;
	exthdr[1] = packet->fragment_eof;
	*(unsigned int*)&exthdr[2] = htonl(packet->seq);
	lengthfieldframeEncode(packet->buf, INNER_BASEHDRSIZE, packet->bodylen + INNER_EXTHDRSIZE);
}

static void innerchannel_reply_ack(ChannelBase_t* c, unsigned int seq, const struct sockaddr* addr) {
	ReactorObject_t* o = c->o;
	unsigned char buf[sizeof(NetPacket_t) + INNER_HDRSIZE];
	NetPacket_t* packet = (NetPacket_t*)buf;

	packet->bodylen = 0;
	packet->hdrlen = INNER_HDRSIZE;
	packet->seq = seq;
	packet->fragment_eof = 1;
	packet->type = NETPACKET_ACK;
	innerchannel_encode(c, packet);
	if (o->m_connected) {
		addr = NULL;
	}
	sendto(o->fd, (char*)packet->buf, packet->hdrlen + packet->bodylen, 0, addr, sockaddrLength(addr));
}

static void innerchannel_recv(ChannelBase_t* c, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* addr) {
	unsigned int cmdid_rpcid_sz = 9;
	if (bodylen >= cmdid_rpcid_sz) {
		UserMsg_t* message = newUserMsg(bodylen - cmdid_rpcid_sz);
		if (!message) {
			return;
		}
		message->be_from_cluster = 1;
		message->channel = c;
		if (!(c->flag & CHANNEL_FLAG_STREAM)) {
			memcpy(&message->peer_addr, addr, sockaddrLength(addr));
		}
		message->rpc_status = *bodyptr;
		message->retcode = message->cmdid = ntohl(*(int*)(bodyptr + 1));
		message->rpcid = ntohl(*(int*)(bodyptr + 5));
		if (message->datalen) {
			memcpy(message->data, bodyptr + cmdid_rpcid_sz, message->datalen);
		}
		if (ptrBSG()->conf->enqueue_timeout_msec > 0) {
			message->enqueue_time_msec = gmtimeMillisecond();
		}
		dataqueuePush(channelUserData(c)->dq, &message->internal._);
	}
	else if (c->flag & CHANNEL_FLAG_SERVER) {
		InnerMsg_t packet;
		makeInnerMsgEmpty(&packet);
		channelbaseSendv(c, packet.iov, sizeof(packet.iov) / sizeof(packet.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
	}
}

static ChannelRWDataProc_t s_inner_data_proc = {
	innerchannel_decode,
	innerchannel_recv,
	innerchannel_encode,
	innerchannel_reply_ack
};

static ChannelUserData_t* init_channel_user_data_inner(ChannelUserDataInner_t* ud, ChannelBase_t* channel, struct DataQueue_t* dq) {
	channelrwInitData(&ud->rw, channel->flag, &s_inner_data_proc);
	channelbaseUseRWData(channel, &ud->rw);
	return initChannelUserData(&ud->_, dq);
}

/**************************************************************************/

static void innerchannel_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, long long ts_msec) {
	ReactorObject_t* listen_o = listen_c->o;
	ChannelBase_t* conn_channel;
	IPString_t ip;
	unsigned short port;
	ReactorObject_t* o = reactorobjectOpen(newfd, listen_o->domain, listen_o->socktype, listen_o->protocol);
	if (!o) {
		socketClose(newfd);
		return;
	}
	conn_channel = openChannelInner(o, CHANNEL_FLAG_SERVER, peer_addr, channelUserData(listen_c)->dq);
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

static void innerchannel_on_heartbeat(ChannelBase_t* c, int heartbeat_times) {
	InnerMsg_t msg;
	makeInnerMsgEmpty(&msg);
	channelbaseSendv(c, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
}

static int innerchannel_on_read(ChannelBase_t* channel, unsigned char* buf, unsigned int len, long long timestamp_msec, const struct sockaddr* from_addr) {
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->flag);
	if (hook_proc->on_read) {
		ChannelUserDataInner_t* ud = (ChannelUserDataInner_t*)channelUserData(channel);
		return hook_proc->on_read(&ud->rw, buf, len, timestamp_msec, from_addr);
	}
	return len;
}

static int innerchannel_on_pre_send(ChannelBase_t* channel, NetPacket_t* packet, long long timestamp_msec) {
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->flag);
	if (hook_proc->on_pre_send) {
		ChannelUserDataInner_t* ud = (ChannelUserDataInner_t*)channelUserData(channel);
		return hook_proc->on_pre_send(&ud->rw, packet, timestamp_msec);
	}
	return 1;
}

static void innerchannel_on_exec(ChannelBase_t* channel, long long timestamp_msec) {
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->flag);
	if (hook_proc->on_exec) {
		ChannelUserDataInner_t* ud = (ChannelUserDataInner_t*)channelUserData(channel);
		hook_proc->on_exec(&ud->rw, timestamp_msec);
	}
}

static void innerchannel_on_free(ChannelBase_t* channel) {
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->flag);
	if (hook_proc->on_free) {
		ChannelUserDataInner_t* ud = (ChannelUserDataInner_t*)channelUserData(channel);
		hook_proc->on_free(&ud->rw);
	}
}

static ChannelBaseProc_t s_inner_proc = {
	defaultChannelOnReg,
	innerchannel_on_exec,
	innerchannel_on_read,
	innerchannel_hdrsize,
	innerchannel_on_pre_send,
	innerchannel_on_heartbeat,
	defaultChannelOnDetach,
	innerchannel_on_free
};

/**************************************************************************/

ChannelBase_t* openChannelInner(ReactorObject_t* o, int flag, const struct sockaddr* addr, struct DataQueue_t* dq) {
	ChannelUserDataInner_t* ud;
	ChannelBase_t* c = channelbaseOpen(sizeof(ChannelBase_t) + sizeof(ChannelUserDataInner_t), flag, o, addr);
	if (!c) {
		return NULL;
	}
	//
	ud = (ChannelUserDataInner_t*)(c + 1);
	channelSetUserData(c, init_channel_user_data_inner(ud, c, dq));
	// c->_.write_fragment_size = 500;
	c->proc = &s_inner_proc;
	flag = c->flag;
	if (flag & CHANNEL_FLAG_CLIENT) {
		c->heartbeat_timeout_sec = 10;
		c->heartbeat_maxtimes = 3;
	}
	else if (flag & CHANNEL_FLAG_SERVER) {
		c->heartbeat_timeout_sec = 20;
	}
	if (flag & CHANNEL_FLAG_STREAM) {
		if ((flag & CHANNEL_FLAG_CLIENT) || (flag & CHANNEL_FLAG_SERVER)) {
			int on = ptrBSG()->conf->tcp_nodelay;
			setsockopt(o->fd, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));
		}
	}
	else {
		c->dgram_ctx.cwndsize = ptrBSG()->conf->udp_cwndsize;
	}
	return c;
}

ChannelBase_t* openListenerInner(int socktype, const char* ip, unsigned short port, struct DataQueue_t* dq) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	ChannelBase_t* c;
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
	c->on_ack_halfconn = innerchannel_accept_callback;
	return c;
}

#ifdef __cplusplus
}
#endif
