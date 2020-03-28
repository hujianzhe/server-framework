#include "global.h"
#include <stdio.h>

/*************************************************************************/
static const unsigned int CHANNEL_BASEHDRSIZE = 4;
static const unsigned int CHANNEL_EXTHDRSIZE = 7;
static unsigned int lengthfieldframe_hdrsize(Channel_t* c, unsigned int bodylen) {
	return CHANNEL_BASEHDRSIZE + CHANNEL_EXTHDRSIZE;
}

static void channel_lengthfieldframe_decode(Channel_t* c, unsigned char* buf, size_t buflen, ChannelInbufDecodeResult_t* decode_result) {
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
		if (datalen < CHANNEL_EXTHDRSIZE)
			decode_result->incomplete = 1;
		else {
			decode_result->pktype = *data;
			decode_result->pkseq = ntohl(*(unsigned int*)(data + 1));
			data += CHANNEL_EXTHDRSIZE;
			datalen -= CHANNEL_EXTHDRSIZE;
		}
		decode_result->bodyptr = data;
		decode_result->bodylen = datalen;
		decode_result->decodelen = res;
	}
}

static void channel_lengthfieldframe_encode(Channel_t* c, unsigned char* hdr, unsigned int bodylen, unsigned char pktype, unsigned int pkseq) {
	bodylen += CHANNEL_EXTHDRSIZE;
	*(hdr + CHANNEL_BASEHDRSIZE) = pktype;
	*(unsigned int*)(hdr + CHANNEL_BASEHDRSIZE + 1) = htonl(pkseq);
	*(unsigned short*)(hdr + CHANNEL_BASEHDRSIZE + 5) = 0;
	lengthfieldframeEncode(hdr, CHANNEL_BASEHDRSIZE, bodylen);
}

static void accept_callback(ChannelBase_t* listen_c, FD_t newfd, const void* peer_addr, long long ts_msec) {
	Channel_t* listen_channel = pod_container_of(listen_c, Channel_t, _);
	ReactorObject_t* listen_o = listen_c->o;
	IPString_t ip;
	unsigned short port;
	ReactorObject_t* o = reactorobjectOpen(newfd, listen_o->domain, listen_o->socktype, listen_o->protocol);
	if (!o) {
		socketClose(newfd);
		return;
	}
	listen_channel = mqsocketOpenChannel(o, CHANNEL_FLAG_SERVER, peer_addr);
	if (!listen_channel) {
		reactorCommitCmd(NULL, &o->freecmd);
		return;
	}
	reactorCommitCmd(selectReactor((size_t)newfd), &o->regcmd);
	if (sockaddrDecode((struct sockaddr_storage*)peer_addr, ip, &port))
		printf("accept new socket(%p), ip:%s, port:%hu\n", o, ip, port);
	else
		puts("accept parse sockaddr error");
}

static void channel_lengthfieldfram_reply_ack(Channel_t* c, unsigned int seq, const void* addr) {
	unsigned int hdrsize = c->on_hdrsize(c, 0);
	unsigned char* buf = (unsigned char*)alloca(hdrsize);
	c->on_encode(c, buf, 0, NETPACKET_ACK, seq);
	socketWrite(c->_.o->fd, buf, hdrsize, 0, addr, sockaddrLength(addr));
}

static void channel_recv(Channel_t* c, const void* addr, ChannelInbufDecodeResult_t* decode_result) {
	/*
	int i;
	for (i = 0; i < decode_result->bodylen; ++i) {
		if (decode_result->bodyptr[i] != i % 255) {
			puts("ERROR !!!!!!! ERROR /1==///////////");
			break;
		}
	}
	printf("bodylen = %d, %d\n", decode_result->bodylen, i);
	*/
	if (decode_result->bodylen >= sizeof(int)) {
		MQRecvMsg_t* message = (MQRecvMsg_t*)malloc(sizeof(MQRecvMsg_t) + decode_result->bodylen - sizeof(int));
		if (!message) {
			return;
		}
		message->internal.type = REACTOR_USER_CMD;
		message->channel = c;
		if (c->_.flag & CHANNEL_FLAG_STREAM) {
			message->peer_addr.sa.sa_family = AF_UNSPEC;
		}
		else {
			memcpy(&message->peer_addr, addr, sockaddrLength(addr));
		}
		message->cmd = ntohl(*(int*)(decode_result->bodyptr));
		message->datalen = decode_result->bodylen - sizeof(int);
		if (message->datalen) {
			memcpy(message->data, decode_result->bodyptr + sizeof(int), message->datalen);
		}
		message->data[message->datalen] = 0;
		dataqueuePush(&g_DataQueue, &message->internal._);
	}
	else if (c->_.flag & CHANNEL_FLAG_SERVER) {
		MQSendMsg_t packet;
		makeMQSendMsgEmpty(&packet);
		channelSendv(c, packet.iov, sizeof(packet.iov) / sizeof(packet.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
		puts("reply a empty packet");
		//puts("not reply empty packet");
	}
}

static void channel_detach(ChannelBase_t* channel) {
	dataqueuePush(&g_DataQueue, &channel->freecmd._);
}

static void channel_reg_handler(ChannelBase_t* c, long long timestamp_msec) {
	Channel_t* channel;
	unsigned short channel_flag;
	IPString_t ip = { 0 };
	unsigned short port = 0;
	const char* socktype_str;
	if (!sockaddrDecode(&c->to_addr.st, ip, &port)) {
		puts("reg parse sockaddr error");
		return;
	}

	channel = pod_container_of(c, Channel_t, _);
	channel_flag = channel->_.flag;
	socktype_str = (channel_flag & CHANNEL_FLAG_STREAM) ? "tcp" : "udp";
	if (channel_flag & CHANNEL_FLAG_CLIENT) {
		printf("connect addr %s(%s:%hu)\n", socktype_str, ip, port);
		channelSendv(channel, NULL, 0, NETPACKET_SYN);
	}
	else if (channel_flag & CHANNEL_FLAG_LISTEN) {
		printf("listen addr %s(%s:%hu)\n", socktype_str, ip, port);
	}
	else if (channel_flag & CHANNEL_FLAG_SERVER) {
		printf("server reg %s(%s:%hu)\n", socktype_str, ip, port);
		channelEnableHeartbeat(channel, timestamp_msec);
	}
}

/**************************************************************************/

Channel_t* mqsocketOpenChannel(ReactorObject_t* o, int flag, const void* saddr) {
	Channel_t* c = reactorobjectOpenChannel(o, flag, 0, saddr);
	if (!c)
		return NULL;
	// c->_.write_fragment_size = 500;
	c->_.on_reg = channel_reg_handler;
	c->_.on_detach = channel_detach;
	c->maxhdrsize = lengthfieldframe_hdrsize(c, 0);
	c->on_hdrsize = lengthfieldframe_hdrsize;
	c->on_decode = channel_lengthfieldframe_decode;
	c->on_encode = channel_lengthfieldframe_encode;
	c->dgram.on_reply_ack = channel_lengthfieldfram_reply_ack;
	c->on_recv = channel_recv;
	if (flag & CHANNEL_FLAG_CLIENT) {
		c->heartbeat_timeout_sec = 10;
		c->heartbeat_maxtimes = 3;
	}
	else if (flag & CHANNEL_FLAG_SERVER)
		c->heartbeat_timeout_sec = 20;
	if (flag & CHANNEL_FLAG_DGRAM)
		c->_.dgram_ctx.cwndsize = 10;
	return c;
}

ReactorObject_t* mqlistenOpen(int domain, int socktype, const char* ip, unsigned short port) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	Channel_t* c;
	if (!sockaddrEncode(&local_saddr.st, domain, ip, port))
		return NULL;
	o = reactorobjectOpen(INVALID_FD_HANDLE, domain, socktype, 0);
	if (!o)
		return NULL;
	if (!socketBindAddr(o->fd, &local_saddr.sa, sockaddrLength(&local_saddr))) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	if (SOCK_STREAM == socktype) {
		if (!socketTcpListen(o->fd)) {
			reactorCommitCmd(NULL, &o->freecmd);
			return NULL;
		}
	}
	c = mqsocketOpenChannel(o, CHANNEL_FLAG_LISTEN, &local_saddr);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	c->_.on_ack_halfconn = accept_callback;
	return o;
}
