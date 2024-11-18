#include "config.h"
#include "global.h"
#include "net_channel_proc_imp.h"
#include "task_thread.h"
#include <stdio.h>

static void stack_co_sche_channel_detach_impl(struct StackCoSche_t* sche, StackCoAsyncParam_t* param) {
	TaskThread_t* t = (TaskThread_t*)StackCoSche_userdata(sche);
	TaskThreadStackCo_t* thrd = pod_container_of(t, TaskThreadStackCo_t, _);
	NetChannel_t* channel = (NetChannel_t*)param->value;
	thrd->net_detach(t, channel);
}

static void stack_co_sche_execute_msg_impl(struct StackCoSche_t* sche, StackCoAsyncParam_t* param) {
	TaskThread_t* t = (TaskThread_t*)StackCoSche_userdata(sche);
	DispatchNetMsg_t* net_msg = (DispatchNetMsg_t*)param->value;
	TaskThreadStackCo_t* thrd = pod_container_of(t, TaskThreadStackCo_t, _);

	NetChannel_add_ref(net_msg->channel);
	thrd->net_dispatch(t, net_msg);
	NetChannel_close_ref(net_msg->channel);
	net_msg->channel = NULL;
}

static void stack_co_sche_channel_detach(void* sche, NetChannel_t* channel) {
	StackCoAsyncParam_t async_param = { 0 };
	async_param.value = channel;
	async_param.fn_value_free = (void(*)(void*))NetChannel_close_ref;
	StackCoSche_function((struct StackCoSche_t*)sche, stack_co_sche_channel_detach_impl, &async_param);
}

static void stack_co_sche_execute_msg(void* sche, DispatchNetMsg_t* msg) {
	StackCoAsyncParam_t async_param = { 0 };
	async_param.value = msg;
	async_param.fn_value_free = (void(*)(void*))freeDispatchNetMsg;
	StackCoSche_function((struct StackCoSche_t*)sche, stack_co_sche_execute_msg_impl, &async_param);
}

static void stack_co_sche_resume_msg(void* sche, DispatchNetMsg_t* msg) {
	StackCoAsyncParam_t async_param = { 0 };
	async_param.value = msg;
	async_param.fn_value_free = (void(*)(void*))freeDispatchNetMsg;
	StackCoSche_resume_block_by_id((struct StackCoSche_t*)sche, msg->rpcid, STACK_CO_STATUS_FINISH, &async_param);
}

static void stack_co_sche_resume(void* sche, int64_t id, int canceled) {
	int status = canceled ? STACK_CO_STATUS_ERROR : STACK_CO_STATUS_FINISH;
	StackCoSche_resume_block_by_id((struct StackCoSche_t*)sche, id, status, NULL);
}

#ifdef __cplusplus
extern "C" {
#endif

const NetScheHook_t* getNetScheHookStackCo(void) {
	static const NetScheHook_t s_NetScheHook_StackCo = {
		stack_co_sche_channel_detach,
		stack_co_sche_execute_msg,
		stack_co_sche_resume_msg,
		stack_co_sche_resume
	};
	return &s_NetScheHook_StackCo;
}

NetChannelUserData_t* initNetChannelUserData(NetChannelUserData_t* ud, const BootServerConfigNetChannelOption_t* channel_opt, void* sche) {
	ud->sche = sche;
	ud->channel_opt = *channel_opt;
	ud->rpc_id_syn_ack = 0;
	ud->text_data_print_log = 0;
	return ud;
}

void defaultNetChannelOnSynAck(NetChannel_t* c, long long ts_msec) {
	NetChannelUserData_t* ud = NetChannel_get_userdata(c);
	if (ud->rpc_id_syn_ack != 0) {
		ptrBSG()->net_sche_hook->on_resume(ud->sche, ud->rpc_id_syn_ack, 0);
		ud->rpc_id_syn_ack = 0;
	}
}

void defaultNetChannelOnDetach(NetChannel_t* c) {
	NetChannelUserData_t* ud = NetChannel_get_userdata(c);
	if (ud->rpc_id_syn_ack != 0) {
		ptrBSG()->net_sche_hook->on_resume(ud->sche, ud->rpc_id_syn_ack, 1);
		ud->rpc_id_syn_ack = 0;
	}
	ptrBSG()->net_sche_hook->on_detach(ud->sche, c);
}

#ifdef __cplusplus
}
#endif
