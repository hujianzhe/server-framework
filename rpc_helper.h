#ifndef RPC_HELPER_H
#define	RPC_HELPER_H

#include "util/inc/component/rpc_core.h"
#include "session.h"

RpcItem_t* newRpcItem(void);
void freeRpcItemWhenTimeout(RpcItem_t* rpc_item);
void freeRpcItemWhenNormal(Session_t* session, RpcItem_t* rpc_item);
void freeRpcItem(RpcItem_t* rpc_item);
RpcItem_t* readyRpcItem(RpcItem_t* rpc_item, Session_t* session, long long timeout_msec);


#endif // !RPC_HELPER_H
