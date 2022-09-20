#include "global.h"

#ifdef __cplusplus
extern "C" {
#endif

static RpcItem_t* new_rpc_item(long long timeout_msec) {
	RpcItem_t* rpc_item = (RpcItem_t*)malloc(sizeof(RpcItem_t));
	if (!rpc_item) {
		return NULL;
	}
	return rpcItemSet(rpc_item, rpcGenId(), gmtimeMillisecond(), timeout_msec);
}

void freeRpcItemWhenNormal(RpcItem_t* rpc_item) {
	free(rpc_item);
}

void freeRpcItemWhenChannelDetach(TaskThread_t* thrd, ChannelBase_t* channel) {
	if (thrd->f_rpc) {
		while (1) {
			RpcItem_t* rpc_item = rpcGetBatchNodeItem(&thrd->f_rpc->base, channel);
			if (!rpc_item) {
				break;
			}
			rpcFiberCoreCancel(thrd->f_rpc, rpc_item);
			free(rpc_item);
		}
	}
	else if (thrd->a_rpc) {
		while (1) {
			RpcItem_t* rpc_item = rpcGetBatchNodeItem(&thrd->a_rpc->base, channel);
			if (!rpc_item) {
				break;
			}
			rpcAsyncCoreCancel(thrd->a_rpc, rpc_item);
			free(rpc_item);
		}
	}
}

RpcItem_t* newRpcItemFiberReady(const void* key, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcItem_t*)) {
	RpcItem_t* rpc_item;
	TaskThread_t* thrd = currentTaskThread();
	if (!thrd) {
		return NULL;
	}
	rpc_item = new_rpc_item(timeout_msec);
	if (!rpc_item) {
		return NULL;
	}
	if (!rpcFiberCoreRegItem(thrd->f_rpc, rpc_item, key, req_arg, ret_callback)) {
		free(rpc_item);
		return NULL;
	}
	return rpc_item;
}

RpcItem_t* newRpcItemAsyncReady(const void* key, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcItem_t*)) {
	RpcItem_t* rpc_item;
	TaskThread_t* thrd = currentTaskThread();
	if (!thrd) {
		return NULL;
	}
	rpc_item = new_rpc_item(timeout_msec);
	if (!rpc_item) {
		return NULL;
	}
	if (!rpcAsyncCoreRegItem(thrd->a_rpc, rpc_item, key, req_arg, ret_callback)) {
		free(rpc_item);
		return NULL;
	}
	return rpc_item;
}

void freeRpcItem(RpcItem_t* rpc_item) {
	TaskThread_t* thrd;
	if (!rpc_item) {
		return;
	}
	thrd = currentTaskThread();
	if (!thrd) {
		return;
	}
	if (thrd->f_rpc) {
		rpcFiberCoreCancel(thrd->f_rpc, rpc_item);
	}
	else if (thrd->a_rpc) {
		rpcAsyncCoreCancel(thrd->a_rpc, rpc_item);
	}
	free(rpc_item);
}

BOOL newFiberSleepMillsecond(long long timeout_msec) {
	RpcItem_t* rpc_item;
	TaskThread_t* thrd = currentTaskThread();
	if (!thrd || !thrd->f_rpc) {
		return FALSE;
	}
	if (timeout_msec <= 0) {
		return TRUE;
	}
	if (thrd->f_rpc->cur_fiber == thrd->f_rpc->sche_fiber) {
		threadSleepMillsecond(timeout_msec);
		return TRUE;
	}
	rpc_item = new_rpc_item(timeout_msec);
	if (!rpc_item) {
		return FALSE;
	}
	if (!rpcFiberCoreRegItem(thrd->f_rpc, rpc_item, NULL, NULL, NULL)) {
		free(rpc_item);
		return FALSE;
	}
	rpcFiberCoreYield(thrd->f_rpc);
	return TRUE;
}

RpcItem_t* newChannelRpcItemFiber(ChannelBase_t* channel, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcItem_t*)) {
	if (!channel->valid) {
		return NULL;
	}
	return newRpcItemFiberReady(channel, timeout_msec, req_arg, ret_callback);
}

RpcItem_t* newChannelRpcItemAsync(ChannelBase_t* channel, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcItem_t*)) {
	if (!channel->valid) {
		return NULL;
	}
	return newRpcItemAsyncReady(channel, timeout_msec, req_arg, ret_callback);
}

RpcItem_t* sendClsndRpcReqFiber(ClusterNode_t* clsnd, InnerMsg_t* msg, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcItem_t*)) {
	RpcItem_t* rpc_item;
	ChannelBase_t* channel = connectClusterNode(clsnd);
	if (!channel) {
		return NULL;
	}
	rpc_item = newChannelRpcItemFiber(channel, timeout_msec, req_arg, ret_callback);
	if (!rpc_item) {
		return NULL;
	}
	msg->rpc_status = RPC_STATUS_REQ;
	msg->htonl_rpcid = htonl(rpc_item->id);
	channelbaseSendv(channel, msg->iov, sizeof(msg->iov) / sizeof(msg->iov[0]), NETPACKET_FRAGMENT);
	return rpc_item;
}

RpcItem_t* sendClsndRpcReqAsync(ClusterNode_t* clsnd, InnerMsg_t* msg, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcItem_t*)) {
	RpcItem_t* rpc_item;
	ChannelBase_t* channel = connectClusterNode(clsnd);
	if (!channel) {
		return NULL;
	}
	rpc_item = newChannelRpcItemAsync(channel, timeout_msec, req_arg, ret_callback);
	if (!rpc_item) {
		return NULL;
	}
	msg->rpc_status = RPC_STATUS_REQ;
	msg->htonl_rpcid = htonl(rpc_item->id);
	channelbaseSendv(channel, msg->iov, sizeof(msg->iov) / sizeof(msg->iov[0]), NETPACKET_FRAGMENT);
	return rpc_item;
}

void dispatchRpcReply(UserMsg_t* req_ctrl, int code, const void* data, unsigned int len) {
	InnerMsg_t msg;
	if (RPC_STATUS_REQ == req_ctrl->rpc_status) {
		makeInnerMsgRpcResp(&msg, req_ctrl->rpcid, code, data, len);
		channelbaseSendv(req_ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	}
}

#ifdef __cplusplus
}
#endif
