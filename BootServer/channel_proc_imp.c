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
	ud->dq = dq;
	ud->rpc_id_syn_ack = 0;
	ud->text_data_print_log = 0;
	return ud;
}

void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec) {
	ChannelUserData_t* ud = channelUserData(c);
	if (ud->rpc_id_syn_ack != 0) {
		UserMsg_t* msg = newUserMsg(0);
		msg->channel = c;
		msg->rpcid = ud->rpc_id_syn_ack;
		msg->rpc_status = RPC_STATUS_RESP;
		dataqueuePush(ud->dq, &msg->internal._);
		ud->rpc_id_syn_ack = 0;
	}
}

void defaultChannelOnReg(ChannelBase_t* c, long long timestamp_msec) {
	unsigned short channel_flag;
	IPString_t ip = { 0 };
	unsigned short port = 0;
	const char* socktype_str;
	if (!sockaddrDecode(&c->to_addr.sa, ip, &port)) {
		logErr(ptrBSG()->log, "%s sockaddr decode error, ip:%s port:%hu", __FUNCTION__, ip, port);
		return;
	}

	channel_flag = c->flag;
	socktype_str = (SOCK_STREAM == c->socktype) ? "tcp" : "udp";
	if (channel_flag & CHANNEL_FLAG_CLIENT) {
		logInfo(ptrBSG()->log, "%s connect addr %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
	}
	else if (channel_flag & CHANNEL_FLAG_LISTEN) {
		logInfo(ptrBSG()->log, "%s listen addr %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
	}
	else if (channel_flag & CHANNEL_FLAG_SERVER) {
		logInfo(ptrBSG()->log, "%s server reg %s(%s:%hu)", __FUNCTION__, socktype_str, ip, port);
	}
}

void defaultChannelOnDetach(ChannelBase_t* c) {
	dataqueuePush(channelUserData(c)->dq, &c->freecmd._);
}

#ifdef __cplusplus
}
#endif
