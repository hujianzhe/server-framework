#ifndef GLOBAL_H
#define	GLOBAL_H

#include "util/inc/all.h"
#include "channel_imp.h"
#include "dispatch.h"
#include "msg_struct.h"
#include "rpc_helper.h"
#include "session.h"
#include <stdlib.h>

#include "mq_cmd.h"
#include "mq_cluster.h"
#include "mq_handler.h"

extern volatile int g_Valid;
extern Thread_t* g_ReactorThreads;
extern Thread_t* g_ReactorAcceptThread;
extern Thread_t g_TaskThread;
extern Reactor_t* g_Reactors;
extern Reactor_t* g_ReactorAccept;
extern DataQueue_t g_DataQueue;
extern size_t g_ReactorCnt;
extern RBTimer_t g_Timer;
extern RBTimer_t g_TimerRpcTimeout;
extern List_t g_ClusterList;
extern Hashtable_t g_ClusterTable;
extern Hashtable_t g_SessionTable;
extern RpcFiberCore_t* g_RpcFiberCore;
extern RpcAsyncCore_t* g_RpcAsyncCore;

int initGlobalResource(void);
void freeGlobalResource(void);
Reactor_t* selectReactor(size_t key);

#endif // !GLOBAL_H
