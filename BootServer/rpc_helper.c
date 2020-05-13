#include "global.h"

RpcFiberCore_t* g_RpcFiberCore;
RpcAsyncCore_t* g_RpcAsyncCore;
RBTimer_t g_TimerRpcTimeout;

#ifdef __cplusplus
extern "C" {
#endif

RpcFiberCore_t* ptr_g_RpcFiberCore(void) { return g_RpcFiberCore; }
RpcAsyncCore_t* ptr_g_RpcAsyncCore(void) { return g_RpcAsyncCore; }

static RpcItem_t* newRpcItem(void) {
	RpcItem_t* rpc_item = (RpcItem_t*)malloc(sizeof(RpcItem_t) + sizeof(RBTimerEvent_t));
	if (rpc_item) {
		rpcItemSet(rpc_item, rpcGenId());
		rpc_item->originator = NULL;
		rpc_item->timeout_ev = NULL;
	}
	return rpc_item;
}

void freeRpcItemWhenTimeout(RpcItem_t* rpc_item) {
	Channel_t* channel = (Channel_t*)rpc_item->originator;
	listRemoveNode(&channel->rpc_itemlist, &rpc_item->listnode);
	free(rpc_item);
}

void freeRpcItemWhenNormal(Channel_t* channel, RpcItem_t* rpc_item) {
	if (channel == rpc_item->originator) {
		listRemoveNode(&channel->rpc_itemlist, &rpc_item->listnode);
		if (rpc_item->timeout_ev)
			rbtimerDelEvent(&g_TimerRpcTimeout, (RBTimerEvent_t*)rpc_item->timeout_ev);
		free(rpc_item);
	}
}

void freeRpcItemWhenChannelDetach(Channel_t* channel) {
	ListNode_t* cur, *next;
	for (cur = channel->rpc_itemlist.head; cur; cur = next) {
		RpcItem_t* rpc_item = pod_container_of(cur, RpcItem_t, listnode);
		next = cur->next;

		if (rpc_item->timeout_ev)
			rbtimerDelEvent(&g_TimerRpcTimeout, (RBTimerEvent_t*)rpc_item->timeout_ev);

		if (g_RpcFiberCore)
			rpcFiberCoreCancel(g_RpcFiberCore, rpc_item);
		else if (g_RpcAsyncCore)
			rpcAsyncCoreCancel(g_RpcAsyncCore, rpc_item);

		free(rpc_item);
	}
	listInit(&channel->rpc_itemlist);
}

static RpcItem_t* readyRpcItem(RpcItem_t* rpc_item, Channel_t* channel, long long timeout_msec) {
	rpc_item->timestamp_msec = gmtimeMillisecond();
	if (timeout_msec >= 0) {
		RBTimerEvent_t* timeout_ev = (RBTimerEvent_t*)(rpc_item + 1);
		timeout_ev->timestamp_msec = rpc_item->timestamp_msec + timeout_msec;
		timeout_ev->callback = (int(*)(struct RBTimerEvent_t*, void*))(size_t)1;
		timeout_ev->arg = rpc_item;
		if (!rbtimerAddEvent(&g_TimerRpcTimeout, timeout_ev)) {
			return NULL;
		}
		rpc_item->timeout_ev = timeout_ev;
	}
	listPushNodeBack(&channel->rpc_itemlist, &rpc_item->listnode);
	rpc_item->originator = channel;
	return rpc_item;
}

RpcItem_t* newRpcItemFiberReady(RpcFiberCore_t* rpc, Channel_t* channel, long long timeout_msec) {
	RpcItem_t* rpc_item = newRpcItem();
	if (!rpc_item)
		return NULL;
	if (!readyRpcItem(rpc_item, channel, timeout_msec)) {
		free(rpc_item);
		return NULL;
	}
	if (!rpcFiberCoreRegItem(rpc, rpc_item)) {
		freeRpcItem(rpc_item);
		return NULL;
	}
	return rpc_item;
}

RpcItem_t* newRpcItemAsyncReady(RpcAsyncCore_t* rpc, Channel_t* channel, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcItem_t*)) {
	RpcItem_t* rpc_item = newRpcItem();
	if (!rpc_item)
		return NULL;
	if (!readyRpcItem(rpc_item, channel, timeout_msec)) {
		free(rpc_item);
		return NULL;
	}
	if (!rpcAsyncCoreRegItem(rpc, rpc_item, req_arg, ret_callback)) {
		freeRpcItem(rpc_item);
		return NULL;
	}
	return rpc_item;
}

void freeRpcItem(RpcItem_t* rpc_item) {
	if (rpc_item->originator) {
		Channel_t* channel = (Channel_t*)rpc_item->originator;
		listRemoveNode(&channel->rpc_itemlist, &rpc_item->listnode);
	}
	if (rpc_item->timeout_ev)
		rbtimerDelEvent(&g_TimerRpcTimeout, (RBTimerEvent_t*)rpc_item->timeout_ev);
	free(rpc_item);
}

#ifdef __cplusplus
}
#endif