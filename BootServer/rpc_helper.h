#ifndef RPC_HELPER_H
#define	RPC_HELPER_H

#include "util/inc/component/rpc_core.h"
#include "session_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll RpcItem_t* newRpcItem(void);
__declspec_dll void freeRpcItemWhenTimeout(RpcItem_t* rpc_item);
__declspec_dll void freeRpcItemWhenNormal(Channel_t* channel, RpcItem_t* rpc_item);
__declspec_dll void freeRpcItem(RpcItem_t* rpc_item);
__declspec_dll RpcItem_t* readyRpcItem(RpcItem_t* rpc_item, Channel_t* channel, long long timeout_msec);

#ifdef __cplusplus
}
#endif

#endif // !RPC_HELPER_H
