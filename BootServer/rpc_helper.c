#include "global.h"

#ifdef __cplusplus
extern "C" {
#endif

static RpcItem_t* newRpcItem(void) {
	RpcItem_t* rpc_item = (RpcItem_t*)malloc(sizeof(RpcItem_t) + sizeof(RBTimerEvent_t));
	if (rpc_item) {
		rpcItemSet(rpc_item, rpcGenId());
		rpc_item->originator = NULL;
		rpc_item->timeout_ev = NULL;
	}
	return rpc_item;
}

void freeRpcItemWhenTimeout(TaskThread_t* thrd, RpcItem_t* rpc_item) {
	Channel_t* channel = (Channel_t*)rpc_item->originator;
	ChannelUserData_t* ud = (ChannelUserData_t*)channel->userdata;
	listRemoveNode(&ud->rpc_itemlist, &rpc_item->listnode);
	free(rpc_item);
}

void freeRpcItemWhenNormal(TaskThread_t* thrd, Channel_t* channel, RpcItem_t* rpc_item) {
	if (channel == rpc_item->originator) {
		ChannelUserData_t* ud = (ChannelUserData_t*)channel->userdata;
		listRemoveNode(&ud->rpc_itemlist, &rpc_item->listnode);
		if (rpc_item->timeout_ev)
			rbtimerDelEvent(&thrd->rpc_timer, (RBTimerEvent_t*)rpc_item->timeout_ev);
		free(rpc_item);
	}
}

void freeRpcItemWhenChannelDetach(TaskThread_t* thrd, Channel_t* channel) {
	ChannelUserData_t* ud = (ChannelUserData_t*)channel->userdata;
	ListNode_t* cur, *next;
	for (cur = ud->rpc_itemlist.head; cur; cur = next) {
		RpcItem_t* rpc_item = pod_container_of(cur, RpcItem_t, listnode);
		next = cur->next;

		if (rpc_item->timeout_ev)
			rbtimerDelEvent(&thrd->rpc_timer, (RBTimerEvent_t*)rpc_item->timeout_ev);

		if (thrd->f_rpc)
			rpcFiberCoreCancel(thrd->f_rpc, rpc_item);
		else if (thrd->a_rpc)
			rpcAsyncCoreCancel(thrd->a_rpc, rpc_item);

		free(rpc_item);
	}
	listInit(&ud->rpc_itemlist);
}

static RpcItem_t* readyRpcItem(TaskThread_t* thrd, RpcItem_t* rpc_item, Channel_t* channel, long long timeout_msec) {
	ChannelUserData_t* ud = (ChannelUserData_t*)channel->userdata;
	rpc_item->timestamp_msec = gmtimeMillisecond();
	if (timeout_msec >= 0) {
		RBTimerEvent_t* timeout_ev = (RBTimerEvent_t*)(rpc_item + 1);
		timeout_ev->timestamp_msec = rpc_item->timestamp_msec + timeout_msec;
		timeout_ev->callback = NULL;
		timeout_ev->arg = rpc_item;
		if (!rbtimerAddEvent(&thrd->rpc_timer, timeout_ev)) {
			return NULL;
		}
		rpc_item->timeout_ev = timeout_ev;
	}
	listPushNodeBack(&ud->rpc_itemlist, &rpc_item->listnode);
	rpc_item->originator = channel;
	return rpc_item;
}

RpcItem_t* newRpcItemFiberReady(TaskThread_t* thrd, Channel_t* channel, long long timeout_msec) {
	RpcItem_t* rpc_item;
	if (!channel->_.valid)
		return NULL;
	rpc_item = newRpcItem();
	if (!rpc_item)
		return NULL;
	if (!readyRpcItem(thrd, rpc_item, channel, timeout_msec)) {
		free(rpc_item);
		return NULL;
	}
	if (!rpcFiberCoreRegItem(thrd->f_rpc, rpc_item)) {
		freeRpcItem(thrd, rpc_item);
		return NULL;
	}
	return rpc_item;
}

RpcItem_t* newRpcItemAsyncReady(TaskThread_t* thrd, Channel_t* channel, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcAsyncCore_t*, RpcItem_t*)) {
	RpcItem_t* rpc_item;
	if (!channel->_.valid)
		return NULL;
	rpc_item = newRpcItem();
	if (!rpc_item)
		return NULL;
	if (!readyRpcItem(thrd, rpc_item, channel, timeout_msec)) {
		free(rpc_item);
		return NULL;
	}
	if (!rpcAsyncCoreRegItem(thrd->a_rpc, rpc_item, req_arg, ret_callback)) {
		freeRpcItem(thrd, rpc_item);
		return NULL;
	}
	return rpc_item;
}

void freeRpcItem(TaskThread_t* thrd, RpcItem_t* rpc_item) {
	if (rpc_item->originator) {
		Channel_t* channel = (Channel_t*)rpc_item->originator;
		ChannelUserData_t* ud = (ChannelUserData_t*)channel->userdata;
		listRemoveNode(&ud->rpc_itemlist, &rpc_item->listnode);
	}
	if (rpc_item->timeout_ev)
		rbtimerDelEvent(&thrd->rpc_timer, (RBTimerEvent_t*)rpc_item->timeout_ev);
	if (thrd->f_rpc)
		rpcFiberCoreCancel(thrd->f_rpc, rpc_item);
	else if (thrd->a_rpc)
		rpcAsyncCoreCancel(thrd->a_rpc, rpc_item);
	free(rpc_item);
}

#ifdef __cplusplus
}
#endif