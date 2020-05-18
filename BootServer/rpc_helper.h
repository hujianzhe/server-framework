#ifndef RPC_HELPER_H
#define	RPC_HELPER_H

#include "util/inc/component/rpc_core.h"
#include "util/inc/component/rbtimer.h"
#include "session_struct.h"

extern RpcFiberCore_t* g_RpcFiberCore;
extern RpcAsyncCore_t* g_RpcAsyncCore;
extern RBTimer_t g_TimerRpcTimeout;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport RpcFiberCore_t* ptr_g_RpcFiberCore(void);
__declspec_dllexport RpcAsyncCore_t* ptr_g_RpcAsyncCore(void);

__declspec_dllexport RpcItem_t* newRpcItemFiberReady(RpcFiberCore_t* rpc, Channel_t* channel, long long timeout_msec);
__declspec_dllexport RpcItem_t* newRpcItemAsyncReady(RpcAsyncCore_t* rpc, Channel_t* channel, long long timeout_msec, void* req_arg, void(*ret_callback)(RpcItem_t*));
__declspec_dllexport void freeRpcItem(RpcItem_t* rpc_item);
void freeRpcItemWhenTimeout(RpcItem_t* rpc_item);
void freeRpcItemWhenNormal(Channel_t* channel, RpcItem_t* rpc_item);
void freeRpcItemWhenChannelDetach(Channel_t* channel);

#ifdef __cplusplus
}
#endif

#endif // !RPC_HELPER_H
