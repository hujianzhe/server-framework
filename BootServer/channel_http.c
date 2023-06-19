#include "global.h"
#include "channel_http.h"

typedef struct ChannelUserDataHttp_t {
	ChannelUserData_t _;
} ChannelUserDataHttp_t;

static ChannelUserData_t* init_channel_user_data_http(ChannelUserDataHttp_t* ud, struct StackCoSche_t* sche) {
	return initChannelUserData(&ud->_, sche);
}

/**************************************************************************/

static void free_user_msg(UserMsg_t* msg) {
	HttpFrame_t* frame = (HttpFrame_t*)msg->param.value;
	free(httpframeReset(frame));
}

static void httpframe_recv(ChannelBase_t* c, HttpFrame_t* httpframe, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* addr) {
	ChannelUserDataHttp_t* ud;
	DispatchCallback_t callback;
	UserMsg_t* message;

	ud = (ChannelUserDataHttp_t*)channelUserData(c);
	if (!ud->_.rpc_id_syn_ack) {
		callback = getStringDispatch(ptrBSG()->dispatch, httpframe->uri);
		if (!callback) {
			free(httpframeReset(httpframe));
			return;
		}
	}
	message = newUserMsg(bodylen);
	if (!message) {
		free(httpframeReset(httpframe));
		return;
	}
	message->channel = c;
	message->param.value = httpframe;
	message->on_free = free_user_msg;
	if (message->datalen) {
		memmove(message->data, bodyptr, message->datalen);
	}
	if (ptrBSG()->conf->enqueue_timeout_msec > 0) {
		message->enqueue_time_msec = gmtimeMillisecond();
	}

	if (!ud->_.rpc_id_syn_ack && httpframe->method[0]) {
		message->callback = callback;
		StackCoSche_function(channelUserData(c)->sche, TaskThread_call_dispatch, message, (void(*)(void*))freeUserMsg);
	}
	else {
		message->rpcid = ud->_.rpc_id_syn_ack;
		ud->_.rpc_id_syn_ack = 0;
		StackCoSche_resume_block_by_id(channelUserData(c)->sche, message->rpcid, STACK_CO_STATUS_FINISH, message, (void(*)(void*))freeUserMsg);
	}
}

static int httpframe_on_read(ChannelBase_t* c, unsigned char* buf, unsigned int buflen, long long timestamp_msec, const struct sockaddr* addr, socklen_t addrlen) {
	int res;
	unsigned int content_length;
	HttpFrame_t* frame = (HttpFrame_t*)malloc(sizeof(HttpFrame_t));
	if (!frame) {
		return -1;
	}
	res = httpframeDecodeHeader(frame, (char*)buf, buflen);
	if (res < 0) {
		free(frame);
		return -1;
	}
	if (0 == res) {
		free(frame);
		return 0;
	}
	content_length = frame->content_length;
	if (content_length) {
		if (content_length > buflen - res) {
			free(httpframeReset(frame));
			return 0;
		}
		if (frame->multipart_form_data_boundary &&
			frame->multipart_form_data_boundary[0])
		{
			if (!httpframeDecodeMultipartFormDataList(frame, buf + res, content_length)) {
				free(httpframeReset(frame));
				return -1;
			}
			frame->uri[frame->pathlen] = 0;
			httpframe_recv(c, frame, NULL, 0, addr);
		}
		else {
			frame->uri[frame->pathlen] = 0;
			httpframe_recv(c, frame, buf + res, content_length, addr);
		}
	}
	else {
		frame->uri[frame->pathlen] = 0;
		httpframe_recv(c, frame, NULL, 0, addr);
	}
	return res + content_length;
}

static void http_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, socklen_t addrlen, long long ts_msec) {
	ChannelBase_t* conn_channel;

	conn_channel = openChannelHttp(CHANNEL_FLAG_SERVER, newfd, peer_addr, channelUserData(listen_c)->sche);
	if (!conn_channel) {
		socketClose(newfd);
		return;
	}
	channelbaseReg(selectReactor(), conn_channel);
}

static void http_channel_on_free(ChannelBase_t* c) {
	ChannelUserDataHttp_t* ud = (ChannelUserDataHttp_t*)channelUserData(c);
	free(ud);
}

static ChannelBaseProc_t s_http_proc = {
	NULL,
	NULL,
	httpframe_on_read,
	NULL,
	NULL,
	NULL,
	defaultChannelOnDetach,
	http_channel_on_free
};

#ifdef __cplusplus
extern "C" {
#endif

ChannelBase_t* openChannelHttp(int flag, FD_t fd, const struct sockaddr* addr, struct StackCoSche_t* sche) {
	ChannelUserDataHttp_t* ud;
	ChannelBase_t* c;

	ud = (ChannelUserDataHttp_t*)malloc(sizeof(ChannelUserDataHttp_t));
	if (!ud) {
		return NULL;
	}
	c = channelbaseOpen(flag, &s_http_proc, fd, addr->sa_family, SOCK_STREAM, 0);
	if (!c) {
		free(ud);
		return NULL;
	}
	channelbaseSetOperatorSockaddr(c, addr, sockaddrLength(addr->sa_family));
	//
	channelSetUserData(c, init_channel_user_data_http(ud, sche));
	flag = c->flag;
	if (flag & CHANNEL_FLAG_SERVER) {
		c->heartbeat_timeout_sec = 20;
	}
	return c;
}

ChannelBase_t* openListenerHttp(const char* ip, unsigned short port, struct StackCoSche_t* sche) {
	Sockaddr_t local_saddr;
	socklen_t local_saddrlen;
	FD_t listen_fd;
	ChannelBase_t* c;
	ChannelUserDataHttp_t* ud = NULL;
	int domain = ipstrFamily(ip);

	if (!sockaddrEncode(&local_saddr.sa, domain, ip, port)) {
		return NULL;
	}
	listen_fd = socket(domain, SOCK_STREAM, 0);
	if (INVALID_FD_HANDLE == listen_fd) {
		return NULL;
	}
	local_saddrlen = sockaddrLength(domain);
	if (!socketTcpListen(listen_fd, &local_saddr.sa, local_saddrlen)) {
		goto err;
	}
	ud = (ChannelUserDataHttp_t*)malloc(sizeof(ChannelUserDataHttp_t));
	if (!ud) {
		goto err;
	}
	c = channelbaseOpen(CHANNEL_FLAG_LISTEN, &s_http_proc, listen_fd, domain, SOCK_STREAM, 0);
	if (!c) {
		goto err;
	}
	channelbaseSetOperatorSockaddr(c, &local_saddr.sa, local_saddrlen);
	c->on_ack_halfconn = http_accept_callback;
	channelSetUserData(c, init_channel_user_data_http(ud, sche));
	return c;
err:
	free(ud);
	socketClose(listen_fd);
	return NULL;
}

#ifdef __cplusplus
}
#endif
