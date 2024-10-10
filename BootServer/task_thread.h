#ifndef BOOT_SERVER_TASK_THREAD_H
#define	BOOT_SERVER_TASK_THREAD_H

#include "util/inc/sysapi/atomic.h"
#include "util/inc/sysapi/process.h"
#include "util/inc/component/stack_co_sche.h"
#include "util/inc/datastruct/random.h"

struct NetChannel_t;
struct DispatchNetMsg_t;
struct StackCoSche_t;
struct TaskThread_t;
struct BootServerConfigSchedulerOption_t;

typedef struct TaskThreadHook_t {
	unsigned int(*entry)(void*);
	void(*exit)(struct TaskThread_t*);
	void(*deleter)(struct TaskThread_t*);
} TaskThreadHook_t;

typedef struct TaskThread_t {
	Thread_t tid;
	RandMT19937_t randmt19937_ctx;
	Atom32_t refcnt;
	Atom8_t already_boot;
	char detached;
	char exited;
	union {
		struct StackCoSche_t* sche_stack_co;
		void* sche;
	};
	const TaskThreadHook_t* hook;
} TaskThread_t;

typedef struct TaskThreadStackCo_t {
	TaskThread_t _;
	void(*net_dispatch)(TaskThread_t* thrd, struct DispatchNetMsg_t* net_msg);
	void(*net_detach)(TaskThread_t* thrd, struct NetChannel_t* channel);
} TaskThreadStackCo_t;

void stopAllTaskThreads(void);
void waitFreeAllTaskThreads(void);

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll TaskThread_t* newTaskThreadStackCo(const struct BootServerConfigSchedulerOption_t* conf_option);
__declspec_dll BOOL saveTaskThread(TaskThread_t* t);
__declspec_dll BOOL runTaskThread(TaskThread_t* t);
__declspec_dll void stopTaskThread(TaskThread_t* t);
__declspec_dll void freeTaskThread(TaskThread_t* t);
__declspec_dll TaskThread_t* currentTaskThread(void);

#ifdef __cplusplus
}
#endif

#endif // !TASK_THREAD_H
