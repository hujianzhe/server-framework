#include "global.h"
#include "config.h"
#include "task_thread.h"
#include <stdio.h>

static unsigned int task_thread_stack_co_entry(void* arg) {
	TaskThread_t* t = (TaskThread_t*)arg;
	while (0 == StackCoSche_sche(t->sche_stack_co, -1));
	return 0;
}

static void task_thread_stack_co_exit(TaskThread_t* t) {
	StackCoSche_exit(t->sche_stack_co);
}

static void task_thread_stack_co_deleter(TaskThread_t* t) {
	TaskThreadStackCo_t* thrd = pod_container_of(t, TaskThreadStackCo_t, _);
	StackCoSche_destroy(thrd->_.sche_stack_co);
	free(thrd);
}

static const TaskThreadHook_t s_TaskThreadStackCoHook = {
	task_thread_stack_co_entry,
	task_thread_stack_co_exit,
	task_thread_stack_co_deleter
};

static void default_net_dispatch(TaskThread_t* thrd, struct DispatchNetMsg_t* net_msg) {
	net_msg->callback(thrd, net_msg);
}
static void ignore_net_detach(TaskThread_t* thrd, struct NetChannel_t* channel) {}

/**************************************************************************************/

static DynArr_t(TaskThread_t*) s_allTaskThreads;
static Atom32_t s_allTaskThreadsSpinLock;

static void __remove_task_thread(TaskThread_t* t) {
	size_t idx;
	while (_xchg32(&s_allTaskThreadsSpinLock, 1));
	dynarrFindValue(&s_allTaskThreads, t, idx);
	if (idx != -1) {
		dynarrRemoveIdx(&s_allTaskThreads, idx);
	}
	_xchg32(&s_allTaskThreadsSpinLock, 0);
}

void freeAllTaskThreads(void) {
	size_t idx;
	while (_xchg32(&s_allTaskThreadsSpinLock, 1));
	for (idx = 0; idx < s_allTaskThreads.len; ++idx) {
		TaskThread_t* t = s_allTaskThreads.buf[idx];
		t->hook->deleter(t);
	}
	dynarrFreeMemory(&s_allTaskThreads);
	_xchg32(&s_allTaskThreadsSpinLock, 0);
}

/**************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

int saveTaskThread(TaskThread_t* t) {
	size_t idx;
	int save_ok = 0;
	while (_xchg32(&s_allTaskThreadsSpinLock, 1));
	dynarrFindValue(&s_allTaskThreads, t, idx);
	if (-1 == idx) {
		dynarrInsert(&s_allTaskThreads, s_allTaskThreads.len, t, save_ok);
	}
	_xchg32(&s_allTaskThreadsSpinLock, 0);
	return save_ok;
}

TaskThread_t* newTaskThreadStackCo(size_t co_stack_size) {
	int sche_ok = 0, seedval;
	TaskThreadStackCo_t* thrd = (TaskThreadStackCo_t*)malloc(sizeof(TaskThreadStackCo_t));
	if (!thrd) {
		return NULL;
	}

	thrd->_.sche_stack_co = StackCoSche_new(co_stack_size, &thrd->_);
	if (!thrd->_.sche_stack_co) {
		goto err;
	}
	sche_ok = 1;

	if (!saveTaskThread(&thrd->_)) {
		goto err;
	}
	thrd->_.hook = &s_TaskThreadStackCoHook;

	seedval = time(NULL);
	mt19937Seed(&thrd->_.randmt19937_ctx, seedval);
	thrd->net_dispatch = default_net_dispatch;
	thrd->net_detach = ignore_net_detach;
	return &thrd->_;
err:
	if (sche_ok) {
		StackCoSche_destroy(thrd->_.sche_stack_co);
	}
	free(thrd);
	return NULL;
}

BOOL runTaskThread(TaskThread_t* t) {
	return threadCreate(&t->tid, t->hook->entry, t);
}

void freeTaskThread(TaskThread_t* t) {
	if (t) {
		__remove_task_thread(t);
		t->hook->deleter(t);
	}
}

TaskThread_t* currentTaskThread(void) {
	Thread_t tid = threadSelf();
	TaskThread_t* thrd = NULL;
	size_t i;
	while (_xchg32(&s_allTaskThreadsSpinLock, 1));
	for (i = 0; i < s_allTaskThreads.len; ++i) {
		thrd = s_allTaskThreads.buf[i];
		if (threadEqual(tid, thrd->tid)) {
			break;
		}
	}
	_xchg32(&s_allTaskThreadsSpinLock, 0);
	return thrd;
}

#ifdef __cplusplus
}
#endif
