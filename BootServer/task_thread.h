#ifndef BOOT_SERVER_TASK_THREAD_H
#define	BOOT_SERVER_TASK_THREAD_H

#include "util/inc/component/dataqueue.h"
#include "util/inc/component/rbtimer.h"
#include "util/inc/component/rpc_core.h"

struct Dispatch_t;
struct ClusterTable_t;

typedef struct TaskThread_t {
	Thread_t tid;
	DataQueue_t dq;
	RBTimer_t timer;
	RBTimer_t rpc_timer;
	RBTimer_t fiber_sleep_timer;
	struct Dispatch_t* dispatch;
	RpcFiberCore_t* f_rpc;
	RpcAsyncCore_t* a_rpc;
	struct ClusterTable_t* clstbl;
	int init_argc;
	char** init_argv;
	int(*fn_init)(struct TaskThread_t* thrd, int argc, char** argv);
	void(*fn_destroy)(struct TaskThread_t* thrd);
	UserMsg_t* __fn_init_fiber_msg;
	const char* errmsg;
} TaskThread_t;

#ifdef __cplusplus
extern "C" {
#endif

TaskThread_t* newTaskThread(void);
BOOL runTaskThread(TaskThread_t* t);
void freeTaskThread(TaskThread_t* t);

#ifdef __cplusplus
}
#endif

#endif // !TASK_THREAD_H