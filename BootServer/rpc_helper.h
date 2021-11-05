#ifndef BOOT_SERVER_RPC_HELPER_H
#define	BOOT_SERVER_RPC_HELPER_H

#include "util/inc/component/rpc_core.h"
#include "util/inc/component/rbtimer.h"
#include "session_struct.h"
#include "task_thread.h"
#include "cluster_node.h"
#include "dispatch.h"
#include "msg_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll RpcItem_t* newRpcItemFiberReady(Channel_t* channel, long long timeout_msec);
__declspec_dll RpcItem_t* newRpcItemAsyncReady(Channel_t* channel, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcAsyncCore_t*, RpcItem_t*));
__declspec_dll void freeRpcItem(RpcItem_t* rpc_item);
__declspec_dll BOOL newFiberSleepMillsecond(long long timeout_msec);

__declspec_dll RpcItem_t* sendClsndRpcReqFiberNoSche(ClusterNode_t* clsnd, InnerMsg_t* msg, long long timeout_msec);
__declspec_dll RpcItem_t* sendClsndRpcReqAsync(ClusterNode_t* clsnd, InnerMsg_t* msg, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcAsyncCore_t*, RpcItem_t*));
__declspec_dll void dispatchRpcReply(UserMsg_t* req_ctrl, int code, const void* data, unsigned int len);

void freeRpcItemWhenNormal(RBTimer_t* rpc_timer, RpcItem_t* rpc_item);
void freeRpcItemWhenChannelDetach(TaskThread_t* thrd, Channel_t* channel);

#ifdef __cplusplus
}
#endif

#endif // !RPC_HELPER_H
