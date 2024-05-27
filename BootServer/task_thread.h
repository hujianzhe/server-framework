#ifndef BOOT_SERVER_TASK_THREAD_H
#define	BOOT_SERVER_TASK_THREAD_H

#include "util/inc/component/stack_co_sche.h"
#include "util/inc/datastruct/random.h"

struct ChannelBase_t;
struct DispatchNetMsg_t;

typedef struct TaskThread_t {
	Thread_t tid;
	struct StackCoSche_t* sche;
	const char* errmsg;
	Rand48_t rand48_ctx;
	RandMT19937_t randmt19937_ctx;
	void(*net_dispatch)(struct TaskThread_t* thrd, struct DispatchNetMsg_t* req_ctrl);
	void(*on_channel_detach)(struct TaskThread_t* thrd, struct ChannelBase_t* channel);
} TaskThread_t;

void TaskThread_channel_base_detach(struct StackCoSche_t* sche, StackCoAsyncParam_t* param);

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll TaskThread_t* newTaskThread(size_t co_stack_size);
__declspec_dll BOOL runTaskThread(TaskThread_t* t);
__declspec_dll void freeTaskThread(TaskThread_t* t);

__declspec_dll TaskThread_t* currentTaskThread(void);
__declspec_dll void TaskThread_net_dispatch(struct StackCoSche_t* sche, StackCoAsyncParam_t* param);

#ifdef __cplusplus
}
#endif

#endif // !TASK_THREAD_H
