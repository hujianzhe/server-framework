#include "global.h"
#include "net_channel_http.h"

typedef struct NetChannelUserDataHttp_t {
	NetChannelUserData_t _;
} NetChannelUserDataHttp_t;

static NetChannelUserData_t* init_channel_user_data_http(NetChannelUserDataHttp_t* ud, const BootServerConfigNetChannelOption_t* channel_opt, void* sche) {
	return initNetChannelUserData(&ud->_, channel_opt, sche);
}

/**************************************************************************/

static void free_user_msg(DispatchNetMsg_t* net_msg) {
	HttpFrame_t* frame = (HttpFrame_t*)net_msg->param.value;
	free(httpframeReset(frame));
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
	message = newDispatchNetMsg(bodylen, 0);
	if (!message) {
		free(httpframeReset(httpframe));
		return;
	}
	message->channel = c;
	message->on_free = free_user_msg;
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
		message->rpcid = ud->_.rpc_id_syn_ack;
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
	defaultNetChannelOnDetach,
	http_channel_on_free
};

static void http_accept_callback(NetChannel_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, socklen_t addrlen, long long ts_msec) {
	NetChannel_t* c = NULL;
	NetChannelUserDataHttp_t* conn_ud = NULL;
	const BootServerConfigNetChannelOption_t* channel_opt;
	NetChannelUserData_t* listen_ud;

	c = NetChannel_open_with_fd(NET_CHANNEL_SIDE_SERVER, &s_http_proc, newfd, peer_addr->sa_family, 0);
	if (!c) {
		socketClose(newfd);
		goto err;
	}
	conn_ud = (NetChannelUserDataHttp_t*)malloc(sizeof(NetChannelUserDataHttp_t));
	if (!conn_ud) {
		goto err;
	}
	listen_ud = NetChannel_get_userdata(listen_c);
	channel_opt = &listen_ud->channel_opt;
	init_channel_user_data_http(conn_ud, channel_opt, listen_ud->sche);
	NetChannel_set_userdata(c, conn_ud);
	c->heartbeat_timeout_msec = channel_opt->heartbeat_timeout_msec > 0 ? channel_opt->heartbeat_timeout_msec : 20000;
	NetChannel_reg(selectNetReactor(), c);
	NetChannel_close_ref(c);
	return;
err:
	free(conn_ud);
	NetChannel_close_ref(c);
}

#ifdef __cplusplus
extern "C" {
#endif

NetChannel_t* openNetChannelHttpClient(const BootServerConfigConnectOption_t* opt, void* sche) {
	Sockaddr_t connect_saddr;
	socklen_t connect_saddrlen;
	NetChannelUserDataHttp_t* ud = NULL;
	NetChannel_t* c = NULL;
	const BootServerConfigNetChannelOption_t* channel_conf_opt = &opt->channel_opt;
	int domain = ipstrFamily(channel_conf_opt->ip);

	connect_saddrlen = sockaddrEncode(&connect_saddr.sa, domain, channel_conf_opt->ip, channel_conf_opt->port);
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
	init_channel_user_data_http(ud, channel_conf_opt, sche);
	NetChannel_set_userdata(c, ud);
	c->heartbeat_timeout_msec = channel_conf_opt->heartbeat_timeout_msec > 0 ? channel_conf_opt->heartbeat_timeout_msec : 10000;
	c->connect_timeout_msec = opt->connect_timeout_msec;
	c->on_syn_ack = defaultNetChannelOnSynAck;
	return c;
err:
	free(ud);
	NetChannel_close_ref(c);
	return NULL;
}

NetChannel_t* openNetListenerHttp(const BootServerConfigListenOption_t* opt, void* sche) {
	Sockaddr_t listen_saddr;
	socklen_t listen_saddrlen;
	NetChannel_t* c = NULL;
	NetChannelUserDataHttp_t* ud = NULL;
	const BootServerConfigNetChannelOption_t* channel_conf_opt = &opt->channel_opt;
	int domain = ipstrFamily(channel_conf_opt->ip);

	listen_saddrlen = sockaddrEncode(&listen_saddr.sa, domain, channel_conf_opt->ip, channel_conf_opt->port);
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
	c->listen_backlog = opt->backlog;
	if (!NetChannel_set_operator_sockaddr(c, &listen_saddr.sa, listen_saddrlen)) {
		goto err;
	}
	c->on_ack_halfconn = http_accept_callback;
	init_channel_user_data_http(ud, channel_conf_opt, sche);
	NetChannel_set_userdata(c, ud);
	return c;
err:
	free(ud);
	NetChannel_close_ref(c);
	return NULL;
}

#ifdef __cplusplus
}
#endif
