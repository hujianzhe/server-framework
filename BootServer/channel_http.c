#include "global.h"
#include "channel_http.h"

typedef struct ChannelUserDataHttp_t {
	ChannelUserData_t _;
	int rpc_id_recv;
} ChannelUserDataHttp_t;

static ChannelUserData_t* init_channel_user_data_http(ChannelUserDataHttp_t* ud, struct StackCoSche_t* sche) {
	ud->rpc_id_recv = 0;
	return initChannelUserData(&ud->_, sche);
}

/**************************************************************************/

static void free_user_msg(UserMsg_t* msg) {
	HttpFrame_t* frame = (HttpFrame_t*)msg->param.value;
	free(httpframeReset(frame));
	free(msg);
}

static void httpframe_recv(ChannelBase_t* c, HttpFrame_t* httpframe, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* addr) {
	ChannelUserDataHttp_t* ud;
	UserMsg_t* message = newUserMsg(bodylen);
	if (!message) {
		free(httpframeReset(httpframe));
		return;
	}
	message->channel = c;
	httpframe->uri[httpframe->pathlen] = 0;
	message->param.value = httpframe;
	message->cmdstr = httpframe->uri;
	message->cmdid = 0;
	message->on_free = free_user_msg;

	ud = (ChannelUserDataHttp_t*)channelUserData(c);
	if (ud->rpc_id_recv != 0) {
		message->rpc_status = RPC_STATUS_RESP;
		message->rpcid = ud->rpc_id_recv;
		ud->rpc_id_recv = 0;
	}
	else {
		message->rpc_status = 0;
		message->rpcid = 0;
	}
	if (message->datalen) {
		memcpy(message->data, bodyptr, message->datalen);
	}
	if (ptrBSG()->conf->enqueue_timeout_msec > 0) {
		message->enqueue_time_msec = gmtimeMillisecond();
	}
	if (RPC_STATUS_RESP == message->rpc_status) {
		StackCoSche_resume_block_by_id(channelUserData(c)->sche, message->rpcid, STACK_CO_STATUS_FINISH, message, (void(*)(void*))message->on_free);
	}
	else {
		StackCoSche_function(channelUserData(c)->sche, TaskThread_call_dispatch, message, (void(*)(void*))message->on_free);
	}
}

static int httpframe_on_read(ChannelBase_t* c, unsigned char* buf, unsigned int buflen, long long timestamp_msec, const struct sockaddr* addr) {
	int res;
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
	if (frame->content_length) {
		if (frame->content_length > buflen - res) {
			free(httpframeReset(frame));
			return 0;
		}
		if (frame->multipart_form_data_boundary &&
			frame->multipart_form_data_boundary[0])
		{
			if (!httpframeDecodeMultipartFormDataList(frame, buf + res, frame->content_length)) {
				free(httpframeReset(frame));
				return -1;
			}
			httpframe_recv(c, frame, NULL, 0, addr);
		}
		else {
			httpframe_recv(c, frame, buf + res, frame->content_length, addr);
		}
	}
	else {
		httpframe_recv(c, frame, NULL, 0, addr);
	}
	return res + frame->content_length;
}

static void http_accept_callback(ChannelBase_t* listen_c, FD_t newfd, const struct sockaddr* peer_addr, long long ts_msec) {
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
	c = channelbaseOpen(flag, &s_http_proc, fd, SOCK_STREAM, addr);
	if (!c) {
		free(ud);
		return NULL;
	}
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
	if (!socketEnableReuseAddr(listen_fd, TRUE)) {
		goto err;
	}
	if (bind(listen_fd, &local_saddr.sa, sockaddrLength(&local_saddr.sa))) {
		goto err;
	}
	if (!socketTcpListen(listen_fd)) {
		goto err;
	}

	ud = (ChannelUserDataHttp_t*)malloc(sizeof(ChannelUserDataHttp_t));
	if (!ud) {
		goto err;
	}
	c = channelbaseOpen(CHANNEL_FLAG_LISTEN, &s_http_proc, listen_fd, SOCK_STREAM, &local_saddr.sa);
	if (!c) {
		goto err;
	}
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