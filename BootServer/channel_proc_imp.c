#include "config.h"
#include "global.h"
#include "channel_proc_imp.h"
#include "task_thread.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

ChannelUserData_t* initChannelUserData(ChannelUserData_t* ud, struct StackCoSche_t* sche) {
	ud->session = NULL;
	ud->sche = sche;
	ud->rpc_id_syn_ack = 0;
	ud->text_data_print_log = 0;
	return ud;
}

void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec) {
	ChannelUserData_t* ud = channelUserData(c);
	if (ud->rpc_id_syn_ack != 0) {
		StackCoSche_resume_co(ud->sche, ud->rpc_id_syn_ack, NULL, NULL);
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
	ChannelUserData_t* ud = channelUserData(c);
	StackCoSche_function(ud->sche, TaskThread_channel_base_detach, c, NULL);
}

#ifdef __cplusplus
}
#endif
