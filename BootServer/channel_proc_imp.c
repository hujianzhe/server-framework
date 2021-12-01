#include "config.h"
#include "global.h"
#include "channel_proc_imp.h"
#include "task_thread.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

ChannelUserData_t* initChannelUserData(ChannelUserData_t* ud, DataQueue_t* dq) {
	ud->session = NULL;
	ud->rpc_syn_ack_item = NULL;
	ud->rpc_recv_item = NULL;
	ud->dq = dq;
	ud->ws_handshake_state = 0;
	return ud;
}

void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec) {
	Channel_t* channel = pod_container_of(c, Channel_t, _);
	ChannelUserData_t* ud = (ChannelUserData_t*)channel->userdata;
	if (1 != c->connected_times) {
		return;
	}
	if (ud->rpc_syn_ack_item) {
		UserMsg_t* msg = newUserMsg(0);
		msg->channel = channel;
		msg->rpcid = ud->rpc_syn_ack_item->id;
		msg->rpc_status = RPC_STATUS_RESP;
		dataqueuePush(ud->dq, &msg->internal._);
		ud->rpc_syn_ack_item = NULL;
	}
}

void defaultChannelOnReg(ChannelBase_t* c, long long timestamp_msec) {
	Channel_t* channel;
	unsigned short channel_flag;
	IPString_t ip = { 0 };
	unsigned short port = 0;
	const char* socktype_str;
	if (!sockaddrDecode(&c->to_addr.sa, ip, &port)) {
		logErr(ptrBSG()->log, "%s sockaddr decode error, ip:%s port:%hu", __FUNCTION__, ip, port);
		return;
	}

	channel = pod_container_of(c, Channel_t, _);
	channel_flag = channel->_.flag;
	socktype_str = (channel_flag & CHANNEL_FLAG_STREAM) ? "tcp" : "udp";
	if (channel_flag & CHANNEL_FLAG_CLIENT) {
		logInfo(ptrBSG()->log, "%s connect addr %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
		channelSendv(channel, NULL, 0, NETPACKET_SYN);
	}
	else if (channel_flag & CHANNEL_FLAG_LISTEN) {
		logInfo(ptrBSG()->log, "%s listen addr %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
	}
	else if (channel_flag & CHANNEL_FLAG_SERVER) {
		logInfo(ptrBSG()->log, "%s server reg %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
	}
}

void defaultChannelOnDetach(ChannelBase_t* c) {
	Channel_t* channel = pod_container_of(c, Channel_t, _);
	dataqueuePush(channelUserData(channel)->dq, &c->freecmd._);
}

#ifdef __cplusplus
}
#endif
