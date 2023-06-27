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
	freeUserMsg(msg);
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
		StackCoSche_function(channelUserData(c)->sche, TaskThread_call_dispatch, message, (void(*)(void*))message->on_free);
	}
	else {
		message->rpcid = ud->_.rpc_id_syn_ack;
		ud->_.rpc_id_syn_ack = 0;
		StackCoSche_resume_block_by_id(channelUserData(c)->sche, message->rpcid, STACK_CO_STATUS_FINISH, message, (void(*)(void*))message->on_free);
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

static void http_channel_on_free(ChannelBase_t* c) {
	ChannelUserDataHttp_t* ud = (ChannelUserDataHttp_t*)channelUserData(c);
	free(ud);
}

static ChannelBaseProc_t s_http_proc = {
	NULL,
	httpframe_on_read,
	NULL,
	NULL,
	NULL,
	defaultChannelOnDetach,
	http_channel_on_free
};

static void http_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, socklen_t addrlen, long long ts_msec) {
	ChannelBase_t* c = NULL;
	ChannelUserDataHttp_t* ud = NULL;

	c = channelbaseOpenWithFD(CHANNEL_SIDE_SERVER, &s_http_proc, newfd, peer_addr->sa_family, 0);
	if (!c) {
		socketClose(newfd);
		goto err;
	}
	ud = (ChannelUserDataHttp_t*)malloc(sizeof(ChannelUserDataHttp_t));
	if (!ud) {
		goto err;
	}
	channelSetUserData(c, init_channel_user_data_http(ud, channelUserData(listen_c)->sche));
	c->heartbeat_timeout_sec = 20;
	channelbaseReg(selectReactor(), c);
	return;
err:
	free(ud);
	channelbaseClose(c);
}

#ifdef __cplusplus
extern "C" {
#endif

ChannelBase_t* openChannelHttpClient(const char* ip, unsigned short port, struct StackCoSche_t* sche) {
	Sockaddr_t connect_saddr;
	ChannelUserDataHttp_t* ud = NULL;
	ChannelBase_t* c = NULL;
	int domain = ipstrFamily(ip);

	if (!sockaddrEncode(&connect_saddr.sa, domain, ip, port)) {
		return NULL;
	}
	ud = (ChannelUserDataHttp_t*)malloc(sizeof(ChannelUserDataHttp_t));
	if (!ud) {
		goto err;
	}
	c = channelbaseOpen(CHANNEL_SIDE_CLIENT, &s_http_proc, domain, SOCK_STREAM, 0);
	if (!c) {
		goto err;
	}
	if (!channelbaseSetOperatorSockaddr(c, &connect_saddr.sa, sockaddrLength(domain))) {
		goto err;
	}
	channelSetUserData(c, init_channel_user_data_http(ud, sche));
	c->heartbeat_timeout_sec = 10;
	return c;
err:
	free(ud);
	channelbaseClose(c);
	return NULL;
}

ChannelBase_t* openListenerHttp(const char* ip, unsigned short port, struct StackCoSche_t* sche) {
	Sockaddr_t listen_saddr;
	ChannelBase_t* c = NULL;
	ChannelUserDataHttp_t* ud = NULL;
	int domain = ipstrFamily(ip);

	if (!sockaddrEncode(&listen_saddr.sa, domain, ip, port)) {
		return NULL;
	}
	ud = (ChannelUserDataHttp_t*)malloc(sizeof(ChannelUserDataHttp_t));
	if (!ud) {
		goto err;
	}
	c = channelbaseOpen(CHANNEL_SIDE_LISTEN, &s_http_proc, domain, SOCK_STREAM, 0);
	if (!c) {
		goto err;
	}
	if (!channelbaseSetOperatorSockaddr(c, &listen_saddr.sa, sockaddrLength(domain))) {
		goto err;
	}
	c->on_ack_halfconn = http_accept_callback;
	channelSetUserData(c, init_channel_user_data_http(ud, sche));
	return c;
err:
	free(ud);
	channelbaseClose(c);
	return NULL;
}

#ifdef __cplusplus
}
#endif
