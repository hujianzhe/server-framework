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
		message->channel = c;
		if (SOCK_STREAM != c->socktype) {
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
		if (RPC_STATUS_RESP == message->rpc_status) {
			StackCoSche_resume_co(channelUserData(c)->sche, message->rpcid, message, (void(*)(void*))message->on_free);
		}
		else if (RPC_STATUS_HAND_SHAKE == message->rpc_status) {
			StackCoSche_function(channelUserData(c)->sche, TaskThread_default_clsnd_handshake, message, (void(*)(void*))message->on_free);
		}
		else {
			StackCoSche_function(channelUserData(c)->sche, TaskThread_call_dispatch, message, (void(*)(void*))message->on_free);
		}
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

static ChannelUserData_t* init_channel_user_data_inner(ChannelUserDataInner_t* ud, ChannelBase_t* channel, struct StackCoSche_t* sche) {
	channelrwInitData(&ud->rw, channel->flag, channel->socktype, &s_inner_data_proc);
	channelbaseUseRWData(channel, &ud->rw);
	return initChannelUserData(&ud->_, sche);
}

/**************************************************************************/

static void innerchannel_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, long long ts_msec) {
	ChannelBase_t* conn_channel;

	conn_channel = openChannelInner(CHANNEL_FLAG_SERVER, newfd, listen_c->socktype, peer_addr, channelUserData(listen_c)->sche);
	if (!conn_channel) {
		socketClose(newfd);
		return;
	}
	channelbaseReg(selectReactor(), conn_channel);
}

static void innerchannel_on_heartbeat(ChannelBase_t* c, int heartbeat_times) {
	InnerMsg_t msg;
	makeInnerMsgEmpty(&msg);
	channelbaseSendv(c, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
}

static int innerchannel_on_read(ChannelBase_t* channel, unsigned char* buf, unsigned int len, long long timestamp_msec, const struct sockaddr* from_addr) {
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->flag, channel->socktype);
	if (hook_proc->on_read) {
		ChannelUserDataInner_t* ud = (ChannelUserDataInner_t*)channelUserData(channel);
		return hook_proc->on_read(&ud->rw, buf, len, timestamp_msec, from_addr);
	}
	return len;
}

static int innerchannel_on_pre_send(ChannelBase_t* channel, NetPacket_t* packet, long long timestamp_msec) {
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->flag, channel->socktype);
	if (hook_proc->on_pre_send) {
		ChannelUserDataInner_t* ud = (ChannelUserDataInner_t*)channelUserData(channel);
		return hook_proc->on_pre_send(&ud->rw, packet, timestamp_msec);
	}
	return 1;
}

static void innerchannel_on_exec(ChannelBase_t* channel, long long timestamp_msec) {
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->flag, channel->socktype);
	if (hook_proc->on_exec) {
		ChannelUserDataInner_t* ud = (ChannelUserDataInner_t*)channelUserData(channel);
		hook_proc->on_exec(&ud->rw, timestamp_msec);
	}
}

static void innerchannel_on_free(ChannelBase_t* channel) {
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->flag, channel->socktype);
	if (hook_proc->on_free) {
		ChannelUserDataInner_t* ud = (ChannelUserDataInner_t*)channelUserData(channel);
		hook_proc->on_free(&ud->rw);
	}
}

static ChannelBaseProc_t s_inner_proc = {
	NULL,
	innerchannel_on_exec,
	innerchannel_on_read,
	innerchannel_hdrsize,
	innerchannel_on_pre_send,
	innerchannel_on_heartbeat,
	defaultChannelOnDetach,
	innerchannel_on_free
};

/**************************************************************************/

ChannelBase_t* openChannelInner(int flag, FD_t fd, int socktype, const struct sockaddr* addr, struct StackCoSche_t* sche) {
	ChannelUserDataInner_t* ud;
	size_t sz = sizeof(ChannelBase_t) + sizeof(ChannelUserDataInner_t);
	ChannelBase_t* c = channelbaseOpen(sz, flag, fd, socktype, 0, addr);
	if (!c) {
		return NULL;
	}
	//
	ud = (ChannelUserDataInner_t*)(c + 1);
	channelSetUserData(c, init_channel_user_data_inner(ud, c, sche));
	c->proc = &s_inner_proc;
	flag = c->flag;
	if (flag & CHANNEL_FLAG_CLIENT) {
		c->heartbeat_timeout_sec = 10;
		c->heartbeat_maxtimes = 3;
	}
	else if (flag & CHANNEL_FLAG_SERVER) {
		c->heartbeat_timeout_sec = 20;
	}
	if (SOCK_DGRAM == c->socktype) {
		c->dgram_ctx.cwndsize = ptrBSG()->conf->udp_cwndsize;
	}
	return c;
}

ChannelBase_t* openListenerInner(int socktype, const char* ip, unsigned short port, struct StackCoSche_t* sche) {
	Sockaddr_t local_saddr;
	FD_t listen_fd;
	ChannelBase_t* c;
	int domain = ipstrFamily(ip);
	if (!sockaddrEncode(&local_saddr.sa, domain, ip, port)) {
		return NULL;
	}
	listen_fd = socket(domain, socktype, 0);
	if (INVALID_FD_HANDLE == listen_fd) {
		return NULL;
	}
	if (!socketEnableReuseAddr(listen_fd, TRUE)) {
		goto err;
	}
	if (!socketEnableReusePort(listen_fd, TRUE)) {
		goto err;
	}
	if (bind(listen_fd, &local_saddr.sa, sockaddrLength(&local_saddr.sa))) {
		goto err;
	}
	if (SOCK_STREAM == socktype) {
		if (!socketTcpListen(listen_fd)) {
			goto err;
		}
	}
	c = openChannelInner(CHANNEL_FLAG_LISTEN, listen_fd, socktype, &local_saddr.sa, sche);
	if (!c) {
		goto err;
	}
	c->on_ack_halfconn = innerchannel_accept_callback;
	return c;
err:
	socketClose(listen_fd);
	return NULL;
}

#ifdef __cplusplus
}
#endif
