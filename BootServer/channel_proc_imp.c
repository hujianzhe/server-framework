#include "config.h"
#include "global.h"
#include "channel_proc_imp.h"
#include "task_thread.h"
#include <stdio.h>

static void channel_base_detach_wrapper(struct StackCoSche_t* sche, StackCoAsyncParam_t* param) {
	TaskThread_channel_detach((TaskThread_t*)StackCoSche_userdata(sche), (ChannelBase_t*)param->value);
}

#ifdef __cplusplus
extern "C" {
#endif

void fnNetDispatchStackCoSche(struct StackCoSche_t* sche, StackCoAsyncParam_t* param) {
	TaskThread_net_dispatch((TaskThread_t*)StackCoSche_userdata(sche), (DispatchNetMsg_t*)param->value);
}

ChannelUserData_t* initChannelUserData(ChannelUserData_t* ud, struct StackCoSche_t* sche) {
	ud->sche = sche;
	ud->rpc_id_syn_ack = 0;
	ud->text_data_print_log = 0;
	return ud;
}

void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec) {
	ChannelUserData_t* ud = channelUserData(c);
	if (ud->rpc_id_syn_ack != 0) {
		StackCoSche_resume_block_by_id(ud->sche, ud->rpc_id_syn_ack, STACK_CO_STATUS_FINISH, NULL);
		ud->rpc_id_syn_ack = 0;
	}
}

void defaultChannelOnDetach(ChannelBase_t* c) {
	StackCoAsyncParam_t async_param = { 0 };
	ChannelUserData_t* ud = channelUserData(c);
	if (ud->rpc_id_syn_ack != 0) {
		StackCoSche_resume_block_by_id(ud->sche, ud->rpc_id_syn_ack, STACK_CO_STATUS_ERROR, NULL);
		ud->rpc_id_syn_ack = 0;
	}
	async_param.value = c;
	StackCoSche_function(ud->sche, channel_base_detach_wrapper, &async_param);
}

#ifdef __cplusplus
}
#endif
