#include "config.h"
#include "global.h"
#include "channel_proc_imp.h"
#include "task_thread.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

ChannelUserData_t* initChannelUserData(ChannelUserData_t* ud, struct StackCoSche_t* sche) {
	ud->sche = sche;
	ud->rpc_id_syn_ack = 0;
	ud->text_data_print_log = 0;
	return ud;
}

void defaultRpcOnSynAck(ChannelBase_t* c, long long ts_msec) {
	ChannelUserData_t* ud = channelUserData(c);
	if (ud->rpc_id_syn_ack != 0) {
		StackCoSche_resume_block_by_id(ud->sche, ud->rpc_id_syn_ack, STACK_CO_STATUS_FINISH, NULL, NULL);
		ud->rpc_id_syn_ack = 0;
	}
}

void defaultChannelOnDetach(ChannelBase_t* c) {
	ChannelUserData_t* ud = channelUserData(c);
	StackCoSche_function(ud->sche, TaskThread_channel_base_detach, c, NULL);
}

#ifdef __cplusplus
}
#endif
