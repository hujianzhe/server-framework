#include "config.h"
#include "global.h"
#include <stdio.h>

/*************************************************************************/
static const unsigned int CHANNEL_BASEHDRSIZE = 4;
static const unsigned int CHANNEL_EXTHDRSIZE = 5;
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
		if (datalen < CHANNEL_EXTHDRSIZE) {
			decode_result->incomplete = 1;
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

static void channel_lengthfieldframe_encode(Channel_t* c, unsigned char* hdr, unsigned int bodylen, unsigned char pktype, unsigned int pkseq) {
	bodylen += CHANNEL_EXTHDRSIZE;
	*(hdr + CHANNEL_BASEHDRSIZE) = pktype;
	*(unsigned int*)(hdr + CHANNEL_BASEHDRSIZE + 1) = htonl(pkseq);
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
	listen_channel = openChannel(o, CHANNEL_FLAG_SERVER, peer_addr);
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
	unsigned int cmdid_rpcid_sz = 9;
	if (decode_result->bodylen >= cmdid_rpcid_sz) {
		UserMsg_t* message = (UserMsg_t*)malloc(sizeof(UserMsg_t) + decode_result->bodylen - cmdid_rpcid_sz);
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
		message->httpframe = NULL;
		message->rpc_status = *(decode_result->bodyptr);
		message->cmdid = ntohl(*(int*)(decode_result->bodyptr + 1));
		message->rpcid = ntohl(*(int*)(decode_result->bodyptr + 5));
		message->datalen = decode_result->bodylen - cmdid_rpcid_sz;
		if (message->datalen) {
			memcpy(message->data, decode_result->bodyptr + cmdid_rpcid_sz, message->datalen);
		}
		message->data[message->datalen] = 0;
		dataqueuePush(&g_DataQueue, &message->internal._);
	}
	else if (c->_.flag & CHANNEL_FLAG_SERVER) {
		SendMsg_t packet;
		makeSendMsgEmpty(&packet);
		channelShardSendv(c, packet.iov, sizeof(packet.iov) / sizeof(packet.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
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
		channelShardSendv(channel, NULL, 0, NETPACKET_SYN);
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

Channel_t* openChannel(ReactorObject_t* o, int flag, const void* saddr) {
	Channel_t* c = reactorobjectOpenChannel(o, flag, 0, saddr);
	if (!c)
		return NULL;
	// c->_.write_fragment_size = 500;
	c->_.on_reg = channel_reg_handler;
	c->_.on_detach = channel_detach;
	c->on_hdrsize = lengthfieldframe_hdrsize;
	c->on_decode = channel_lengthfieldframe_decode;
	c->on_encode = channel_lengthfieldframe_encode;
	c->dgram.on_reply_ack = channel_lengthfieldfram_reply_ack;
	c->on_recv = channel_recv;
	flag = c->_.flag;
	if (flag & CHANNEL_FLAG_CLIENT) {
		c->heartbeat_timeout_sec = 10;
		c->heartbeat_maxtimes = 3;
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

ReactorObject_t* openListener(int domain, int socktype, const char* ip, unsigned short port) {
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
	c = openChannel(o, CHANNEL_FLAG_LISTEN, &local_saddr);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	c->_.on_ack_halfconn = accept_callback;
	return o;
}

/**************************************************************************/

static unsigned int httpframe_hdrsize(Channel_t* c, unsigned int bodylen) { return 0; }

static void httpframe_encode(Channel_t* c, unsigned char* hdr, unsigned int bodylen, unsigned char pktype, unsigned int pkseq) {}

static void httpframe_decode(Channel_t* c, unsigned char* buf, size_t buflen, ChannelInbufDecodeResult_t* decode_result) {
	int res;
	HttpFrame_t* frame = (HttpFrame_t*)malloc(sizeof(HttpFrame_t));
	if (!frame) {
		decode_result->err = 1;
		return;
	}
	res = httpframeDecode(frame, buf, buflen);
	if (res < 0) {
		decode_result->err = 1;
		free(frame);
	}
	else if (0 == res) {
		decode_result->incomplete = 1;
		free(frame);
	}
	else {
		if (!strcmp(frame->method, "GET")) {
			decode_result->bodyptr = NULL;
			decode_result->bodylen = 0;
			decode_result->decodelen = res;
			decode_result->userdata = frame;
			return;
		}
		if (!strcmp(frame->method, "POST")) {
			unsigned int content_length;
			const char* content_length_field = httpframeGetHeader(frame, "Content-Length");
			if (!content_length_field) {
				decode_result->err = 1;
				free(httpframeReset(frame));
				return;
			}
			if (sscanf(content_length_field, "%u", &content_length) != 1) {
				decode_result->err = 1;
				free(httpframeReset(frame));
				return;
			}
			if (content_length > buflen - res) {
				decode_result->incomplete = 1;
				/*
				* TODO optimized
				decode_result->decodelen = res;
				c->decode_userdata = frame;
				*/
				return;
			}
			decode_result->bodylen = content_length;
			decode_result->bodyptr = content_length ? (buf + res) : NULL;
			decode_result->decodelen = res + content_length;
			decode_result->userdata = frame;
			return;
		}
		decode_result->err = 1;
		free(httpframeReset(frame));
	}
}

static void httpframe_recv(Channel_t* c, const void* addr, ChannelInbufDecodeResult_t* decode_result) {
	HttpFrame_t* httpframe = (HttpFrame_t*)decode_result->userdata;
	UserMsg_t* message = (UserMsg_t*)malloc(sizeof(UserMsg_t) + decode_result->bodylen);
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
	message->httpframe = httpframe;
	message->rpc_status = 0;
	message->cmdid = 0;
	message->rpcid = 0;
	message->datalen = decode_result->bodylen;
	if (message->datalen) {
		memcpy(message->data, decode_result->bodyptr, message->datalen);
	}
	message->data[message->datalen] = 0;
	dataqueuePush(&g_DataQueue, &message->internal._);
}

static void http_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const void* peer_addr, long long ts_msec) {
	Channel_t* listen_channel = pod_container_of(listen_c, Channel_t, _);
	ReactorObject_t* listen_o = listen_c->o;
	IPString_t ip;
	unsigned short port;
	ReactorObject_t* o = reactorobjectOpen(newfd, listen_o->domain, listen_o->socktype, listen_o->protocol);
	if (!o) {
		socketClose(newfd);
		return;
	}
	listen_channel = openChannelHttp(o, CHANNEL_FLAG_SERVER, peer_addr);
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

Channel_t* openChannelHttp(ReactorObject_t* o, int flag, const void* saddr) {
	Channel_t* c = reactorobjectOpenChannel(o, flag, 0, saddr);
	if (!c)
		return NULL;
	// c->_.write_fragment_size = 500;
	c->_.on_reg = channel_reg_handler;
	c->_.on_detach = channel_detach;
	c->on_hdrsize = httpframe_hdrsize;
	c->on_decode = httpframe_decode;
	c->on_encode = httpframe_encode;
	c->on_recv = httpframe_recv;
	flag = c->_.flag;
	if (flag & CHANNEL_FLAG_CLIENT) {
		c->heartbeat_timeout_sec = 10;
		c->heartbeat_maxtimes = 3;
	}
	else if (flag & CHANNEL_FLAG_SERVER)
		c->heartbeat_timeout_sec = 20;
	return c;
}

ReactorObject_t* openListenerHttp(int domain, const char* ip, unsigned short port) {
	Sockaddr_t local_saddr;
	ReactorObject_t* o;
	Channel_t* c;
	if (!sockaddrEncode(&local_saddr.st, domain, ip, port))
		return NULL;
	o = reactorobjectOpen(INVALID_FD_HANDLE, domain, SOCK_STREAM, 0);
	if (!o)
		return NULL;
	if (!socketBindAddr(o->fd, &local_saddr.sa, sockaddrLength(&local_saddr))) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	if (!socketTcpListen(o->fd)) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	c = openChannelHttp(o, CHANNEL_FLAG_LISTEN, &local_saddr);
	if (!c) {
		reactorCommitCmd(NULL, &o->freecmd);
		return NULL;
	}
	c->_.on_ack_halfconn = http_accept_callback;
	return o;
}