#include "global.h"

#ifdef __cplusplus
extern "C" {
#endif

static RpcItem_t* new_rpc_item(void) {
	RpcItem_t* rpc_item = (RpcItem_t*)malloc(sizeof(RpcItem_t) + sizeof(RBTimerEvent_t));
	if (!rpc_item) {
		return NULL;
	}
	rpcItemSet(rpc_item, rpcGenId());
	rpc_item->timeout_ev = NULL;
	rpc_item->timestamp_msec = gmtimeMillisecond();
	return rpc_item;
}

void freeRpcItemWhenNormal(RBTimer_t* rpc_timer, RpcItem_t* rpc_item) {
	if (rpc_item->timeout_ev) {
		rbtimerDelEvent(rpc_timer, (RBTimerEvent_t*)rpc_item->timeout_ev);
	}
	free(rpc_item);
}

void freeRpcItemWhenChannelDetach(TaskThread_t* thrd, Channel_t* channel) {
	List_t rpcitemlist;
	ListNode_t* cur, *next;

	listInit(&rpcitemlist);
	if (thrd->f_rpc) {
		rpcRemoveBatchNode(&thrd->f_rpc->base, channel, &rpcitemlist);
	}
	else if (thrd->a_rpc) {
		rpcRemoveBatchNode(&thrd->a_rpc->base, channel, &rpcitemlist);
	}
	else {
		return;
	}
	for (cur = rpcitemlist.head; cur; cur = next) {
		RpcItem_t* rpc_item = pod_container_of(cur, RpcItem_t, m_listnode);
		next = cur->next;

		if (rpc_item->timeout_ev) {
			rbtimerDelEvent(&thrd->rpc_timer, (RBTimerEvent_t*)rpc_item->timeout_ev);
		}

		if (thrd->f_rpc) {
			rpcFiberCoreCancel(thrd->f_rpc, rpc_item);
		}
		else if (thrd->a_rpc) {
			rpcAsyncCoreCancel(thrd->a_rpc, rpc_item);
		}

		free(rpc_item);
	}
}

static RpcItem_t* ready_rpc_item(TaskThread_t* thrd, RpcItem_t* rpc_item, long long timeout_msec) {
	if (timeout_msec >= 0) {
		RBTimerEvent_t* ev = (RBTimerEvent_t*)(rpc_item + 1);
		ev->timestamp_msec = rpc_item->timestamp_msec + timeout_msec;
		ev->callback = NULL;
		ev->arg = rpc_item;
		if (!rbtimerAddEvent(&thrd->rpc_timer, ev)) {
			return NULL;
		}
		rpc_item->timeout_ev = ev;
	}
	return rpc_item;
}

RpcItem_t* newRpcItemFiberReady(TaskThread_t* thrd, Channel_t* channel, long long timeout_msec) {
	RpcItem_t* rpc_item;
	if (!channel->_.valid) {
		return NULL;
	}
	rpc_item = new_rpc_item();
	if (!rpc_item) {
		return NULL;
	}
	if (!ready_rpc_item(thrd, rpc_item, timeout_msec)) {
		free(rpc_item);
		return NULL;
	}
	if (!rpcFiberCoreRegItem(thrd->f_rpc, rpc_item, channel)) {
		freeRpcItem(thrd, rpc_item);
		return NULL;
	}
	return rpc_item;
}

RpcItem_t* newRpcItemAsyncReady(TaskThread_t* thrd, Channel_t* channel, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcAsyncCore_t*, RpcItem_t*)) {
	RpcItem_t* rpc_item;
	if (!channel->_.valid) {
		return NULL;
	}
	rpc_item = new_rpc_item();
	if (!rpc_item) {
		return NULL;
	}
	if (!ready_rpc_item(thrd, rpc_item, timeout_msec)) {
		free(rpc_item);
		return NULL;
	}
	if (!rpcAsyncCoreRegItem(thrd->a_rpc, rpc_item, channel, req_arg, ret_callback)) {
		freeRpcItem(thrd, rpc_item);
		return NULL;
	}
	return rpc_item;
}

void freeRpcItem(TaskThread_t* thrd, RpcItem_t* rpc_item) {
	if (!rpc_item) {
		return;
	}
	if (rpc_item->timeout_ev) {
		rbtimerDelEvent(&thrd->rpc_timer, (RBTimerEvent_t*)rpc_item->timeout_ev);
	}
	if (thrd->f_rpc) {
		rpcFiberCoreCancel(thrd->f_rpc, rpc_item);
	}
	else if (thrd->a_rpc) {
		rpcAsyncCoreCancel(thrd->a_rpc, rpc_item);
	}
	free(rpc_item);
}

BOOL newFiberSleepMillsecond(TaskThread_t* thrd, long long timeout_msec) {
	RpcItem_t* rpc_item;
	RBTimerEvent_t* timeout_ev;
	if (!thrd->f_rpc) {
		return FALSE;
	}
	if (timeout_msec <= 0) {
		return TRUE;
	}
	if (thrd->f_rpc->cur_fiber == thrd->f_rpc->sche_fiber) {
		threadSleepMillsecond(timeout_msec);
		return TRUE;
	}
	rpc_item = new_rpc_item();
	if (!rpc_item) {
		return FALSE;
	}
	rpc_item->timestamp_msec = gmtimeMillisecond();
	timeout_ev = (RBTimerEvent_t*)(rpc_item + 1);
	timeout_ev->timestamp_msec = rpc_item->timestamp_msec + timeout_msec;
	timeout_ev->callback = NULL;
	timeout_ev->arg = rpc_item;
	if (!rbtimerAddEvent(&thrd->fiber_sleep_timer, timeout_ev)) {
		free(rpc_item);
		return FALSE;
	}
	rpc_item->timeout_ev = timeout_ev;
	if (!rpcFiberCoreRegItem(thrd->f_rpc, rpc_item, NULL)) {
		rbtimerDelEvent(&thrd->fiber_sleep_timer, timeout_ev);
		free(rpc_item);
		return FALSE;
	}
	rpc_item = rpcFiberCoreYield(thrd->f_rpc);
	free(rpc_item);
	return TRUE;
}

#ifdef __cplusplus
}
#endif
