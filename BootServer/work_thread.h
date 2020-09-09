#ifndef WORK_THREAD_H
#define	WORK_THREAD_H

#include "util/inc/component/dataqueue.h"
#include "util/inc/component/rbtimer.h"
#include "util/inc/component/rpc_core.h"

struct Dispatch_t;

typedef struct TaskThread_t {
	Thread_t tid;
	DataQueue_t dq;
	RBTimer_t timer;
	RBTimer_t rpc_timer;
	RBTimer_t fiber_sleep_timer;
	struct Dispatch_t* dispatch;
	RpcFiberCore_t* f_rpc;
	RpcAsyncCore_t* a_rpc;
} TaskThread_t;

extern TaskThread_t* g_TaskThread;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport TaskThread_t* ptr_g_TaskThread(void);

TaskThread_t* newTaskThread(void);
BOOL runTaskThread(TaskThread_t* t);
void freeTaskThread(TaskThread_t* t);

#ifdef __cplusplus
}
#endif

#endif // !WORK_THREAD_H
