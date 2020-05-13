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

__declspec_dll RpcFiberCore_t* ptr_g_RpcFiberCore(void);
__declspec_dll RpcAsyncCore_t* ptr_g_RpcAsyncCore(void);

__declspec_dll RpcItem_t* newRpcItem(void);
void freeRpcItemWhenTimeout(RpcItem_t* rpc_item);
void freeRpcItemWhenNormal(Channel_t* channel, RpcItem_t* rpc_item);
void freeRpcItemWhenChannelDetach(Channel_t* channel);
__declspec_dll void freeRpcItem(RpcItem_t* rpc_item);
__declspec_dll RpcItem_t* readyRpcItem(RpcItem_t* rpc_item, Channel_t* channel, long long timeout_msec);

#ifdef __cplusplus
}
#endif

#endif // !RPC_HELPER_H
