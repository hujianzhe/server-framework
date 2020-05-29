#ifndef WORK_THREAD_H
#define	WORK_THREAD_H

#include "util/inc/component/dataqueue.h"
#include "util/inc/component/rbtimer.h"
#include "util/inc/component/rpc_core.h"

typedef struct TaskThread_t {
	Thread_t tid;
	DataQueue_t dq;
	RBTimer_t timer;
	RBTimer_t rpc_timer;
	RpcFiberCore_t* f_rpc;
	RpcAsyncCore_t* a_rpc;
} TaskThread_t;

TaskThread_t* newTaskThread(void);
void freeTaskThread(TaskThread_t* t);

#endif // !WORK_THREAD_H
