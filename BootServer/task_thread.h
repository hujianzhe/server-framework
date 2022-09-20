#ifndef BOOT_SERVER_TASK_THREAD_H
#define	BOOT_SERVER_TASK_THREAD_H

#include "util/inc/component/dataqueue.h"
#include "util/inc/component/rbtimer.h"
#include "util/inc/component/rpc_core.h"
#include "util/inc/datastruct/random.h"
#include "dispatch.h"

struct ChannelBase_t;
struct Dispatch_t;
struct ClusterTable_t;

typedef struct TaskThread_t {
	Thread_t tid;
	DataQueue_t dq;
	RBTimer_t timer;
	struct Dispatch_t* dispatch;
	RpcFiberCore_t* f_rpc;
	RpcAsyncCore_t* a_rpc;
	struct ClusterTable_t* clstbl;
	int init_argc;
	char** init_argv;
	int(*fn_init)(struct TaskThread_t* thrd, int argc, char** argv);
	void(*fn_destroy)(struct TaskThread_t* thrd);
	const char* errmsg;
	Rand48_t rand48_ctx;
	RandMT19937_t randmt19937_ctx;
	void(*filter_callback)(struct TaskThread_t* thrd, DispatchCallback_t callback, UserMsg_t* req_ctrl);
	void(*on_channel_detach)(struct TaskThread_t* thrd, struct ChannelBase_t* channel);
} TaskThread_t;

#ifdef __cplusplus
extern "C" {
#endif

TaskThread_t* newTaskThread(void);
BOOL runTaskThread(TaskThread_t* t);
void freeTaskThread(TaskThread_t* t);
__declspec_dll TaskThread_t* currentTaskThread(void);

#ifdef __cplusplus
}
#endif

#endif // !TASK_THREAD_H
