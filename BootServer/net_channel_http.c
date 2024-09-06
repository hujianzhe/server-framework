#include "global.h"
#include "net_channel_http.h"

typedef struct NetChannelUserDataHttp_t {
	NetChannelUserData_t _;
} NetChannelUserDataHttp_t;

static NetChannelUserData_t* init_channel_user_data_http(NetChannelUserDataHttp_t* ud, struct StackCoSche_t* sche) {
	return initChannelUserData(&ud->_, sche);
}

/**************************************************************************/

static void free_user_msg(DispatchBaseMsg_t* msg) {
	DispatchNetMsg_t* net_msg = pod_container_of(msg, DispatchNetMsg_t, base);
	HttpFrame_t* frame = (HttpFrame_t*)net_msg->param.value;
	free(httpframeReset(frame));
	freeDispatchNetMsg(msg);
}

static void httpframe_recv(NetChannel_t* c, HttpFrame_t* httpframe, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* addr) {
	NetChannelUserDataHttp_t* ud;
	DispatchNetCallback_t callback;
	DispatchNetMsg_t* message;

	ud = (NetChannelUserDataHttp_t*)NetChannel_get_userdata(c);
	if (!ud->_.rpc_id_syn_ack) {
		callback = getStringDispatch(ptrBSG()->dispatch, httpframe->uri);
		if (!callback) {
			free(httpframeReset(httpframe));
			return;
		}
	}
	message = newDispatchNetMsg(c, bodylen, free_user_msg);
	if (!message) {
		free(httpframeReset(httpframe));
		return;
	}
	message->param.value = httpframe;
	if (message->datalen) {
		memmove(message->data, bodyptr, message->datalen);
	}
	if (ptrBSG()->conf->enqueue_timeout_msec > 0) {
		message->enqueue_time_msec = gmtimeMillisecond();
	}
	if (!ud->_.rpc_id_syn_ack && httpframe->method[0]) {
		message->callback = callback;
		ptrBSG()->net_sche_hook->on_execute_msg(NetChannel_get_userdata(c)->sche, message);
	}
	else {
		message->base.rpcid = ud->_.rpc_id_syn_ack;
		ud->_.rpc_id_syn_ack = 0;
		ptrBSG()->net_sche_hook->on_resume_msg(NetChannel_get_userdata(c)->sche, message);
	}
}

static int httpframe_on_read(NetChannel_t* c, unsigned char* buf, unsigned int buflen, long long timestamp_msec, const struct sockaddr* addr, socklen_t addrlen) {
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

static void http_channel_on_free(NetChannel_t* c) {
	NetChannelUserDataHttp_t* ud = (NetChannelUserDataHttp_t*)NetChannel_get_userdata(c);
	free(ud);
}

static NetChannelProc_t s_http_proc = {
	NULL,
	httpframe_on_read,
	NULL,
	NULL,
	NULL,
	defaultChannelOnDetach,
	http_channel_on_free
};

static void http_accept_callback(NetChannel_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, socklen_t addrlen, long long ts_msec) {
	NetChannel_t* c = NULL;
	NetChannelUserDataHttp_t* ud = NULL;

	c = NetChannel_open_with_fd(NET_CHANNEL_SIDE_SERVER, &s_http_proc, newfd, peer_addr->sa_family, 0);
	if (!c) {
		socketClose(newfd);
		goto err;
	}
	ud = (NetChannelUserDataHttp_t*)malloc(sizeof(NetChannelUserDataHttp_t));
	if (!ud) {
		goto err;
	}
	NetChannel_set_userdata(c, init_channel_user_data_http(ud, NetChannel_get_userdata(listen_c)->sche));
	c->heartbeat_timeout_sec = 20;
	NetChannel_reg(selectNetReactor(), c);
	NetChannel_close_ref(c);
	return;
err:
	free(ud);
	NetChannel_close_ref(c);
}

#ifdef __cplusplus
extern "C" {
#endif

NetChannel_t* openNetChannelHttpClient(const char* ip, unsigned short port, void* sche) {
	Sockaddr_t connect_saddr;
	socklen_t connect_saddrlen;
	NetChannelUserDataHttp_t* ud = NULL;
	NetChannel_t* c = NULL;
	int domain = ipstrFamily(ip);

	connect_saddrlen = sockaddrEncode(&connect_saddr.sa, domain, ip, port);
	if (connect_saddrlen <= 0) {
		return NULL;
	}
	ud = (NetChannelUserDataHttp_t*)malloc(sizeof(NetChannelUserDataHttp_t));
	if (!ud) {
		goto err;
	}
	c = NetChannel_open(NET_CHANNEL_SIDE_CLIENT, &s_http_proc, domain, SOCK_STREAM, 0);
	if (!c) {
		goto err;
	}
	if (!NetChannel_set_operator_sockaddr(c, &connect_saddr.sa, connect_saddrlen)) {
		goto err;
	}
	NetChannel_set_userdata(c, init_channel_user_data_http(ud, sche));
	c->heartbeat_timeout_sec = 10;
	return c;
err:
	free(ud);
	NetChannel_close_ref(c);
	return NULL;
}

NetChannel_t* openNetListenerHttp(const char* ip, unsigned short port, void* sche) {
	Sockaddr_t listen_saddr;
	socklen_t listen_saddrlen;
	NetChannel_t* c = NULL;
	NetChannelUserDataHttp_t* ud = NULL;
	int domain = ipstrFamily(ip);

	listen_saddrlen = sockaddrEncode(&listen_saddr.sa, domain, ip, port);
	if (listen_saddrlen <= 0) {
		return NULL;
	}
	ud = (NetChannelUserDataHttp_t*)malloc(sizeof(NetChannelUserDataHttp_t));
	if (!ud) {
		goto err;
	}
	c = NetChannel_open(NET_CHANNEL_SIDE_LISTEN, &s_http_proc, domain, SOCK_STREAM, 0);
	if (!c) {
		goto err;
	}
	if (!NetChannel_set_operator_sockaddr(c, &listen_saddr.sa, listen_saddrlen)) {
		goto err;
	}
	c->on_ack_halfconn = http_accept_callback;
	NetChannel_set_userdata(c, init_channel_user_data_http(ud, sche));
	return c;
err:
	free(ud);
	NetChannel_close_ref(c);
	return NULL;
}

#ifdef __cplusplus
}
#endif
