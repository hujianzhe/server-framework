#ifndef RPC_HELPER_H
#define	RPC_HELPER_H

#include "util/inc/component/rpc_core.h"
#include "util/inc/component/rbtimer.h"
#include "session_struct.h"
#include "work_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport RpcItem_t* newRpcItemFiberReady(TaskThread_t* thrd, Channel_t* channel, long long timeout_msec);
__declspec_dllexport RpcItem_t* newRpcItemAsyncReady(TaskThread_t* thrd, Channel_t* channel, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcAsyncCore_t*, RpcItem_t*));
__declspec_dllexport void freeRpcItem(TaskThread_t* thrd, RpcItem_t* rpc_item);
void freeRpcItemWhenTimeout(TaskThread_t* thrd, RpcItem_t* rpc_item);
void freeRpcItemWhenNormal(TaskThread_t* thrd, Channel_t* channel, RpcItem_t* rpc_item);
void freeRpcItemWhenChannelDetach(TaskThread_t* thrd, Channel_t* channel);

#ifdef __cplusplus
}
#endif

#endif // !RPC_HELPER_H
