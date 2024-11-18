#include "global.h"
#include "net_channel_inner.h"

typedef struct NetChannelUserDataInner_t {
	NetChannelUserData_t _;
	NetChannelExData_t rw;
} NetChannelUserDataInner_t;

#ifdef __cplusplus
extern "C" {
#endif

static unsigned int innerchannel_hdrsize(NetChannel_t* c, unsigned int bodylen) { return INNER_MSG_FORMAT_HDRSIZE; }

static void innerchannel_decode(NetChannel_t* c, unsigned char* buf, size_t buflen, NetChannelInbufDecodeResult_t* decode_result) {
	unsigned char* data;
	unsigned int datalen;
	int res = lengthfieldframeDecode(INNER_MSG_FORMAT_BASEHDRSIZE, buf, buflen, &data, &datalen);
	if (res < 0) {
		decode_result->err = 1;
	}
	else if (0 == res) {
		decode_result->incomplete = 1;
	}
	else {
		if (datalen < INNER_MSG_FORMAT_EXTHDRSIZE) {
			decode_result->err = 1;
			return;
		}
		decode_result->pktype = data[0];
		decode_result->fragment_eof = data[1];
		decode_result->pkseq = ntohl(*(unsigned int*)&data[2]);
		data += INNER_MSG_FORMAT_EXTHDRSIZE;
		datalen -= INNER_MSG_FORMAT_EXTHDRSIZE;

		decode_result->bodyptr = data;
		decode_result->bodylen = datalen;
		decode_result->decodelen = res;
	}
}

static void innerchannel_encode(NetChannel_t* c, NetPacket_t* packet) {
	unsigned char* exthdr = packet->buf + INNER_MSG_FORMAT_BASEHDRSIZE;
	exthdr[0] = packet->type;
	exthdr[1] = packet->fragment_eof;
	*(unsigned int*)&exthdr[2] = htonl(packet->seq);
	lengthfieldframeEncode(packet->buf, INNER_MSG_FORMAT_BASEHDRSIZE, packet->bodylen + INNER_MSG_FORMAT_EXTHDRSIZE);
}

static void innerchannel_reply_ack(NetChannel_t* c, unsigned int seq, const struct sockaddr* addr, socklen_t addrlen) {
	NetReactorObject_t* o = c->o;
	unsigned char buf[sizeof(NetPacket_t) + INNER_MSG_FORMAT_HDRSIZE];
	NetPacket_t* packet = (NetPacket_t*)buf;

	packet->bodylen = 0;
	packet->hdrlen = INNER_MSG_FORMAT_HDRSIZE;
	packet->seq = seq;
	packet->fragment_eof = 1;
	packet->type = NETPACKET_ACK;
	innerchannel_encode(c, packet);
	if (o->m_connected) {
		addr = NULL;
	}
	sendto(o->niofd.fd, (char*)packet->buf, packet->hdrlen + packet->bodylen, 0, addr, addrlen);
}

static void innerchannel_recv(NetChannel_t* c, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* addr, socklen_t addrlen) {
	unsigned int hsz = 13;
	if (bodylen >= hsz) {
		DispatchNetMsg_t* message;
		char rpc_status = *bodyptr;

		if (INNER_MSG_FIELD_RPC_STATUS_RESP == rpc_status) {
			message = newDispatchNetMsg(c, bodylen - hsz);
			if (!message) {
				return;
			}
			message->retcode = ntohl(*(int*)(bodyptr + 1));
		}
		else {
			int cmd = ntohl(*(int*)(bodyptr + 1));
			DispatchNetCallback_t callback = getNumberDispatch(ptrBSG()->dispatch, cmd);
			if (!callback) {
				return;
			}
			message = newDispatchNetMsg(c, bodylen - hsz);
			if (!message) {
				return;
			}
			message->callback = callback;
		}

		if (SOCK_STREAM != c->socktype) {
			memmove(&message->peer_addr, addr, addrlen);
			message->peer_addrlen = addrlen;
		}
		message->rpcid = ntohll(*(int64_t*)(bodyptr + 5));
		if (message->datalen) {
			memmove(message->data, bodyptr + hsz, message->datalen);
		}
		if (ptrBSG()->conf->enqueue_timeout_msec > 0) {
			message->enqueue_time_msec = gmtimeMillisecond();
		}
		if (INNER_MSG_FIELD_RPC_STATUS_RESP == rpc_status) {
			ptrBSG()->net_sche_hook->on_resume_msg(NetChannel_get_userdata(c)->sche, message);
		}
		else {
			ptrBSG()->net_sche_hook->on_execute_msg(NetChannel_get_userdata(c)->sche, message);
		}
	}
	else if (!c->heartbeat_sender) {
		InnerMsgPayload_t packet;
		makeInnerMsgEmpty(&packet);
		NetChannel_sendv(c, packet.iov, sizeof(packet.iov) / sizeof(packet.iov[0]), NETPACKET_NO_ACK_FRAGMENT, addr, addrlen);
	}
}

static NetChannelExProc_t s_inner_data_proc = {
	innerchannel_decode,
	innerchannel_recv,
	innerchannel_encode,
	innerchannel_reply_ack
};

static NetChannelUserData_t* init_channel_user_data_inner(NetChannelUserDataInner_t* ud, NetChannel_t* channel, const BootServerConfigNetChannelOption_t* channel_opt, void* sche) {
	NetChannelEx_init(channel, &ud->rw, &s_inner_data_proc);
	return initNetChannelUserData(&ud->_, channel_opt, sche);
}

/**************************************************************************/

static void innerchannel_on_heartbeat(NetChannel_t* c, int heartbeat_times) {
	InnerMsgPayload_t msg;
	makeInnerMsgEmpty(&msg);
	NetChannel_sendv(c, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_NO_ACK_FRAGMENT, NULL, 0);
}

static int innerchannel_on_read(NetChannel_t* channel, unsigned char* buf, unsigned int len, long long timestamp_msec, const struct sockaddr* from_addr, socklen_t addrlen) {
	const NetChannelExHookProc_t* hook_proc = NetChannelEx_get_hook(channel->side, channel->socktype);
	if (hook_proc->on_read) {
		return hook_proc->on_read(channel, buf, len, timestamp_msec, from_addr, addrlen);
	}
	return len;
}

static int innerchannel_on_pre_send(NetChannel_t* channel, NetPacket_t* packet, long long timestamp_msec) {
	const NetChannelExHookProc_t* hook_proc = NetChannelEx_get_hook(channel->side, channel->socktype);
	if (hook_proc->on_pre_send) {
		return hook_proc->on_pre_send(channel, packet, timestamp_msec);
	}
	return 1;
}

static void innerchannel_on_exec(NetChannel_t* channel, long long timestamp_msec) {
	const NetChannelExHookProc_t* hook_proc = NetChannelEx_get_hook(channel->side, channel->socktype);
	if (hook_proc->on_exec) {
		hook_proc->on_exec(channel, timestamp_msec);
	}
}

static void innerchannel_on_free(NetChannel_t* channel) {
	NetChannelUserDataInner_t* ud = (NetChannelUserDataInner_t*)NetChannel_get_userdata(channel);
	const NetChannelExHookProc_t* hook_proc = NetChannelEx_get_hook(channel->side, channel->socktype);
	if (hook_proc->on_free) {
		hook_proc->on_free(channel);
	}
	free(ud);
}

static NetChannelProc_t s_inner_proc = {
	innerchannel_on_exec,
	innerchannel_on_read,
	innerchannel_hdrsize,
	innerchannel_on_pre_send,
	innerchannel_on_heartbeat,
	defaultNetChannelOnDetach,
	innerchannel_on_free
};

static void innerchannel_set_opt(NetChannel_t* c, const BootServerConfigNetChannelOption_t* opt) {
	if (NET_CHANNEL_SIDE_CLIENT == c->side) {
		c->heartbeat_timeout_msec = opt->heartbeat_timeout_msec > 0 ? opt->heartbeat_timeout_msec : 10000;
		c->heartbeat_max_times = opt->readcache_max_size > 0 ? opt->readcache_max_size : 3;
		c->readcache_max_size = opt->readcache_max_size;
		c->sendcache_max_size = opt->sendcache_max_size;
	}
	else if (NET_CHANNEL_SIDE_SERVER == c->side) {
		c->heartbeat_timeout_msec = opt->heartbeat_timeout_msec > 0 ? opt->heartbeat_timeout_msec : 20000;
		c->readcache_max_size = opt->readcache_max_size;
		c->sendcache_max_size = opt->sendcache_max_size;
	}
	if (SOCK_DGRAM == c->socktype) {
		c->dgram_ctx.cwndsize = ptrBSG()->conf->udp_cwndsize;
	}
}

static void innerchannel_accept_callback(NetChannel_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, socklen_t addrlen, long long ts_msec) {
	NetChannel_t* c = NULL;
	NetChannelUserDataInner_t* conn_ud = NULL;
	const NetChannelUserData_t* listen_ud;

	c = NetChannel_open_with_fd(NET_CHANNEL_SIDE_SERVER, &s_inner_proc, newfd, peer_addr->sa_family, 0);
	if (!c) {
		socketClose(newfd);
		goto err;
	}
	conn_ud = (NetChannelUserDataInner_t*)malloc(sizeof(NetChannelUserDataInner_t));
	if (!conn_ud) {
		goto err;
	}
	listen_ud = NetChannel_get_userdata(listen_c);
	init_channel_user_data_inner(conn_ud, c, &listen_ud->channel_opt, listen_ud->sche);
	NetChannel_set_userdata(c, conn_ud);
	innerchannel_set_opt(c, &listen_ud->channel_opt);
	NetChannel_reg(selectNetReactor(), c);
	NetChannel_close_ref(c);
	return;
err:
	free(conn_ud);
	NetChannel_close_ref(c);
}

/**************************************************************************/

NetChannel_t* openNetChannelInnerClient(const BootServerConfigConnectOption_t* opt, void* sche) {
	Sockaddr_t connect_saddr;
	socklen_t connect_saddrlen;
	NetChannel_t* c = NULL;
	NetChannelUserDataInner_t* ud = NULL;
	int domain = ipstrFamily(opt->channel_opt.ip);

	connect_saddrlen = sockaddrEncode(&connect_saddr.sa, domain, opt->channel_opt.ip, opt->channel_opt.port);
	if (connect_saddrlen <= 0) {
		goto err;
	}
	ud = (NetChannelUserDataInner_t*)malloc(sizeof(NetChannelUserDataInner_t));
	if (!ud) {
		goto err;
	}
	c = NetChannel_open(NET_CHANNEL_SIDE_CLIENT, &s_inner_proc, domain, opt->channel_opt.socktype, 0);
	if (!c) {
		goto err;
	}
	if (!NetChannel_set_operator_sockaddr(c, &connect_saddr.sa, connect_saddrlen)) {
		goto err;
	}
	init_channel_user_data_inner(ud, c, &opt->channel_opt, sche);
	NetChannel_set_userdata(c, ud);
	innerchannel_set_opt(c, &opt->channel_opt);
	c->connect_timeout_msec = opt->connect_timeout_msec;
	c->on_syn_ack = defaultNetChannelOnSynAck;
	return c;
err:
	free(ud);
	NetChannel_close_ref(c);
	return NULL;
}

NetChannel_t* openNetListenerInner(const BootServerConfigListenOption_t* opt, void* sche) {
	Sockaddr_t listen_saddr;
	socklen_t listen_saddrlen;
	NetChannel_t* c = NULL;
	NetChannelUserDataInner_t* ud = NULL;
	int domain = ipstrFamily(opt->channel_opt.ip);

	listen_saddrlen = sockaddrEncode(&listen_saddr.sa, domain, opt->channel_opt.ip, opt->channel_opt.port);
	if (listen_saddrlen <= 0) {
		goto err;
	}
	ud = (NetChannelUserDataInner_t*)malloc(sizeof(NetChannelUserDataInner_t));
	if (!ud) {
		goto err;
	}
	c = NetChannel_open(NET_CHANNEL_SIDE_LISTEN, &s_inner_proc, domain, opt->channel_opt.socktype, 0);
	if (!c) {
		goto err;
	}
	c->listen_backlog = opt->backlog;
	if (!NetChannel_set_operator_sockaddr(c, &listen_saddr.sa, listen_saddrlen)) {
		goto err;
	}
	init_channel_user_data_inner(ud, c, &opt->channel_opt, sche);
	NetChannel_set_userdata(c, ud);
	innerchannel_set_opt(c, &opt->channel_opt);
	c->on_ack_halfconn = innerchannel_accept_callback;
	return c;
err:
	free(ud);
	NetChannel_close_ref(c);
	return NULL;
}

InnerMsgPayload_t* makeInnerMsgEmpty(InnerMsgPayload_t* msg) {
	size_t i;
	for (i = 0; i < sizeof(msg->iov) / sizeof(msg->iov[0]); ++i) {
		iobufPtr(msg->iov + i) = NULL;
		iobufLen(msg->iov + i) = 0;
	}
	return msg;
}

InnerMsgPayload_t* makeInnerMsg(InnerMsgPayload_t* msg, int cmdid, const void* data, unsigned int len) {
	msg->htonl_cmdid = htonl(cmdid);
	msg->rpc_status = 0;
	msg->htonll_rpcid = 0;
	iobufPtr(msg->iov + 0) = (char*)&msg->rpc_status;
	iobufLen(msg->iov + 0) = sizeof(msg->rpc_status);
	iobufPtr(msg->iov + 1) = (char*)&msg->htonl_cmdid;
	iobufLen(msg->iov + 1) = sizeof(msg->htonl_cmdid);
	iobufPtr(msg->iov + 2) = (char*)&msg->htonll_rpcid;
	iobufLen(msg->iov + 2) = sizeof(msg->htonll_rpcid);
	if (data && len) {
		iobufPtr(msg->iov + 3) = (char*)data;
		iobufLen(msg->iov + 3) = len;
	}
	else {
		iobufPtr(msg->iov + 3) = NULL;
		iobufLen(msg->iov + 3) = 0;
	}
	return msg;
}

InnerMsgPayload_t* makeInnerMsgRpcReq(InnerMsgPayload_t* msg, int64_t rpcid, int cmdid, const void* data, unsigned int len) {
	makeInnerMsg(msg, cmdid, data, len);
	msg->rpc_status = INNER_MSG_FIELD_RPC_STATUS_REQ;
	msg->htonll_rpcid = htonll(rpcid);
	return msg;
}

InnerMsgPayload_t* makeInnerMsgRpcResp(InnerMsgPayload_t* msg, int64_t rpcid, int retcode, const void* data, unsigned int len) {
	makeInnerMsg(msg, retcode, data, len);
	msg->rpc_status = INNER_MSG_FIELD_RPC_STATUS_RESP;
	msg->htonll_rpcid = htonll(rpcid);
	return msg;
}

#ifdef __cplusplus
}
#endif
