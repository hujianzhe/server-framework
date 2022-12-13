#ifndef BOOT_SERVER_TASK_THREAD_H
#define	BOOT_SERVER_TASK_THREAD_H

#include "util/inc/component/stack_co_sche.h"
#include "util/inc/datastruct/random.h"
#include "dispatch.h"

struct ChannelBase_t;
struct ClusterTable_t;

typedef struct TaskThread_t {
	Thread_t tid;
	struct StackCoSche_t* sche;
	struct Dispatch_t* dispatch;
	struct ClusterTable_t* clstbl;
	const char* errmsg;
	Rand48_t rand48_ctx;
	RandMT19937_t randmt19937_ctx;
	void(*filter_callback)(struct TaskThread_t* thrd, DispatchCallback_t callback, UserMsg_t* req_ctrl);
	void(*on_channel_detach)(struct TaskThread_t* thrd, struct ChannelBase_t* channel);
} TaskThread_t;

void TaskThread_channel_base_detach(struct StackCoSche_t* sche, void* arg);
void TaskThread_default_clsnd_handshake(struct StackCoSche_t* sche, void* arg);

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll TaskThread_t* newTaskThread(size_t co_stack_size);
__declspec_dll BOOL runTaskThread(TaskThread_t* t);
__declspec_dll void freeTaskThread(TaskThread_t* t);

__declspec_dll TaskThread_t* currentTaskThread(void);
__declspec_dll void TaskThread_call_dispatch(struct StackCoSche_t* sche, void* arg);

#ifdef __cplusplus
}
#endif

#endif // !TASK_THREAD_H
