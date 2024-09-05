#ifndef BOOT_SERVER_TASK_THREAD_H
#define	BOOT_SERVER_TASK_THREAD_H

#include "util/inc/component/stack_co_sche.h"
#include "util/inc/datastruct/random.h"

struct NetChannel_t;
struct DispatchNetMsg_t;

typedef struct TaskThread_t {
	Thread_t tid;
	struct StackCoSche_t* sche;
	RandMT19937_t randmt19937_ctx;
	void(*net_dispatch)(struct TaskThread_t* thrd, struct DispatchNetMsg_t* req_ctrl);
} TaskThread_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll TaskThread_t* newTaskThread(size_t co_stack_size);
__declspec_dll BOOL runTaskThread(TaskThread_t* t);
__declspec_dll void freeTaskThread(TaskThread_t* t);

__declspec_dll TaskThread_t* currentTaskThread(void);
__declspec_dll void execNetDispatchOnTaskThread(TaskThread_t* thrd, DispatchNetMsg_t* net_msg);
__declspec_dll void execChannelDetachOnTaskThread(NetChannel_t* channel);

#ifdef __cplusplus
}
#endif

#endif // !TASK_THREAD_H
