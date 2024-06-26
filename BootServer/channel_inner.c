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

static void innerchannel_reply_ack(ChannelBase_t* c, unsigned int seq, const struct sockaddr* addr, socklen_t addrlen) {
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
	sendto(o->niofd.fd, (char*)packet->buf, packet->hdrlen + packet->bodylen, 0, addr, addrlen);
}

static void innerchannel_recv(ChannelBase_t* c, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* addr, socklen_t addrlen) {
	unsigned int hsz = 9;
	if (bodylen >= hsz) {
		DispatchNetMsg_t* message;
		StackCoAsyncParam_t async_param;
		char rpc_status = *bodyptr;

		if (RPC_STATUS_RESP == rpc_status) {
			message = newDispatchNetMsg(c, bodylen - hsz, freeDispatchNetMsg);
			if (!message) {
				return;
			}
			message->retcode = ntohl(*(int*)(bodyptr + 1));
		}
		else {
			int cmd = ntohl(*(int*)(bodyptr + 1));
			DispatchCallback_t callback = getNumberDispatch(ptrBSG()->dispatch, cmd);
			if (!callback) {
				return;
			}
			message = newDispatchNetMsg(c, bodylen - hsz, freeDispatchNetMsg);
			if (!message) {
				return;
			}
			message->callback = callback;
		}

		if (SOCK_STREAM != c->socktype) {
			memmove(&message->peer_addr, addr, addrlen);
			message->peer_addrlen = addrlen;
		}
		message->base.rpcid = ntohl(*(int*)(bodyptr + 5));
		if (message->datalen) {
			memmove(message->data, bodyptr + hsz, message->datalen);
		}
		if (ptrBSG()->conf->enqueue_timeout_msec > 0) {
			message->enqueue_time_msec = gmtimeMillisecond();
		}

		memset(&async_param, 0, sizeof(async_param));
		async_param.value = message;
		async_param.fn_value_free = (void(*)(void*))message->base.on_free;
		if (RPC_STATUS_RESP == rpc_status) {
			StackCoSche_resume_block_by_id(channelUserData(c)->sche, message->base.rpcid, STACK_CO_STATUS_FINISH, &async_param);
		}
		else {
			StackCoSche_function(channelUserData(c)->sche, TaskThread_net_dispatch, &async_param);
		}
	}
	else if (CHANNEL_SIDE_SERVER == c->side) {
		InnerMsg_t packet;
		makeInnerMsgEmpty(&packet);
		channelbaseSendv(c, packet.iov, sizeof(packet.iov) / sizeof(packet.iov[0]), NETPACKET_NO_ACK_FRAGMENT, addr, addrlen);
	}
}

static ChannelRWDataProc_t s_inner_data_proc = {
	innerchannel_decode,
	innerchannel_recv,
	innerchannel_encode,
	innerchannel_reply_ack
};

static ChannelUserData_t* init_channel_user_data_inner(ChannelUserDataInner_t* ud, ChannelBase_t* channel, struct StackCoSche_t* sche) {
	channelbaseUseRWData(channel, &ud->rw, &s_inner_data_proc);
	return initChannelUserData(&ud->_, sche);
}

/**************************************************************************/

static void innerchannel_on_heartbeat(ChannelBase_t* c, int heartbeat_times) {
	InnerMsg_t msg;
	makeInnerMsgEmpty(&msg);
	channelbaseSendv(c, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_NO_ACK_FRAGMENT, NULL, 0);
}

static int innerchannel_on_read(ChannelBase_t* channel, unsigned char* buf, unsigned int len, long long timestamp_msec, const struct sockaddr* from_addr, socklen_t addrlen) {
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->side, channel->socktype);
	if (hook_proc->on_read) {
		return hook_proc->on_read(channel, buf, len, timestamp_msec, from_addr, addrlen);
	}
	return len;
}

static int innerchannel_on_pre_send(ChannelBase_t* channel, NetPacket_t* packet, long long timestamp_msec) {
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->side, channel->socktype);
	if (hook_proc->on_pre_send) {
		return hook_proc->on_pre_send(channel, packet, timestamp_msec);
	}
	return 1;
}

static void innerchannel_on_exec(ChannelBase_t* channel, long long timestamp_msec) {
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->side, channel->socktype);
	if (hook_proc->on_exec) {
		hook_proc->on_exec(channel, timestamp_msec);
	}
}

static void innerchannel_on_free(ChannelBase_t* channel) {
	ChannelUserDataInner_t* ud = (ChannelUserDataInner_t*)channelUserData(channel);
	const ChannelRWHookProc_t* hook_proc = channelrwGetHookProc(channel->side, channel->socktype);
	if (hook_proc->on_free) {
		hook_proc->on_free(channel);
	}
	free(ud);
}

static ChannelBaseProc_t s_inner_proc = {
	innerchannel_on_exec,
	innerchannel_on_read,
	innerchannel_hdrsize,
	innerchannel_on_pre_send,
	innerchannel_on_heartbeat,
	defaultChannelOnDetach,
	innerchannel_on_free
};

static void innerchannel_set_opt(ChannelBase_t* c) {
	if (CHANNEL_SIDE_CLIENT == c->side) {
		c->heartbeat_timeout_sec = 10;
		c->heartbeat_maxtimes = 3;
	}
	else if (CHANNEL_SIDE_SERVER == c->side) {
		c->heartbeat_timeout_sec = 20;
	}
	if (SOCK_DGRAM == c->socktype) {
		c->dgram_ctx.cwndsize = ptrBSG()->conf->udp_cwndsize;
	}
}

static void innerchannel_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, socklen_t addrlen, long long ts_msec) {
	ChannelBase_t* c = NULL;
	ChannelUserDataInner_t* ud = NULL;

	c = channelbaseOpenWithFD(CHANNEL_SIDE_SERVER, &s_inner_proc, newfd, peer_addr->sa_family, 0);
	if (!c) {
		socketClose(newfd);
		goto err;
	}
	ud = (ChannelUserDataInner_t*)malloc(sizeof(ChannelUserDataInner_t));
	if (!ud) {
		goto err;
	}
	channelSetUserData(c, init_channel_user_data_inner(ud, c, channelUserData(listen_c)->sche));
	innerchannel_set_opt(c);
	channelbaseReg(selectReactor(), c);
	channelbaseCloseRef(c);
	return;
err:
	free(ud);
	channelbaseCloseRef(c);
}

/**************************************************************************/

ChannelBase_t* openChannelInnerClient(int socktype, const char* ip, unsigned short port, struct StackCoSche_t* sche) {
	Sockaddr_t connect_saddr;
	socklen_t connect_saddrlen;
	ChannelBase_t* c = NULL;
	ChannelUserDataInner_t* ud = NULL;
	int domain = ipstrFamily(ip);

	connect_saddrlen = sockaddrEncode(&connect_saddr.sa, domain, ip, port);
	if (connect_saddrlen <= 0) {
		goto err;
	}
	ud = (ChannelUserDataInner_t*)malloc(sizeof(ChannelUserDataInner_t));
	if (!ud) {
		goto err;
	}
	c = channelbaseOpen(CHANNEL_SIDE_CLIENT, &s_inner_proc, domain, socktype, 0);
	if (!c) {
		goto err;
	}
	if (!channelbaseSetOperatorSockaddr(c, &connect_saddr.sa, connect_saddrlen)) {
		goto err;
	}
	channelSetUserData(c, init_channel_user_data_inner(ud, c, sche));
	innerchannel_set_opt(c);
	return c;
err:
	free(ud);
	channelbaseCloseRef(c);
	return NULL;
}

ChannelBase_t* openListenerInner(int socktype, const char* ip, unsigned short port, struct StackCoSche_t* sche) {
	Sockaddr_t listen_saddr;
	socklen_t listen_saddrlen;
	ChannelBase_t* c = NULL;
	ChannelUserDataInner_t* ud = NULL;
	int domain = ipstrFamily(ip);

	listen_saddrlen = sockaddrEncode(&listen_saddr.sa, domain, ip, port);
	if (listen_saddrlen <= 0) {
		goto err;
	}
	ud = (ChannelUserDataInner_t*)malloc(sizeof(ChannelUserDataInner_t));
	if (!ud) {
		goto err;
	}
	c = channelbaseOpen(CHANNEL_SIDE_LISTEN, &s_inner_proc, domain, socktype, 0);
	if (!c) {
		goto err;
	}
	if (!channelbaseSetOperatorSockaddr(c, &listen_saddr.sa, listen_saddrlen)) {
		goto err;
	}
	channelSetUserData(c, init_channel_user_data_inner(ud, c, sche));
	innerchannel_set_opt(c);
	c->on_ack_halfconn = innerchannel_accept_callback;
	return c;
err:
	free(ud);
	channelbaseCloseRef(c);
	return NULL;
}

#ifdef __cplusplus
}
#endif
