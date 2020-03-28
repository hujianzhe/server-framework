#ifndef GLOBAL_H
#define	GLOBAL_H

#include "util/inc/all.h"
#include "mq_cmd.h"
#include "mq_cluster.h"
#include "mq_dispatch.h"
#include "mq_socket.h"
#include "mq_msg.h"
#include "mq_session.h"
#include "mq_handler.h"
#include <stdlib.h>

extern volatile int g_Valid;
extern Thread_t* g_ReactorThreads;
extern Thread_t* g_ReactorAcceptThread;
extern Thread_t g_TaskThread;
extern Reactor_t* g_Reactors;
extern Reactor_t* g_ReactorAccept;
extern DataQueue_t g_DataQueue;
extern Fiber_t* g_DataFiber;
extern size_t g_ReactorCnt;
extern RBTimer_t g_Timer;
extern Hashtable_t g_DispatchTable;
extern List_t g_ClusterList;
extern Hashtable_t g_ClusterTable;
extern Hashtable_t g_SessionTable;

int initGlobalResource(void);
void freeGlobalResource(void);
Reactor_t* selectReactor(size_t key);

#endif // !GLOBAL_H
