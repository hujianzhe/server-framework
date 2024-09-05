#include "global.h"
#include "config.h"
#include "task_thread.h"
#include <stdio.h>

static unsigned int task_thread_stack_co_entry(void* arg) {
	TaskThread_t* t = (TaskThread_t*)arg;
	while (0 == StackCoSche_sche(t->sche, -1));
	return 0;
}

static void task_thread_stack_co_exit(TaskThread_t* t) {
	StackCoSche_exit(t->sche);
}

static void task_thread_stack_co_deleter(TaskThread_t* t) {
	TaskThreadStackCo_t* thrd = pod_container_of(t, TaskThreadStackCo_t, _);
	StackCoSche_destroy(thrd->_.sche);
	free(thrd);
}

/**************************************************************************************/

static DynArr_t(TaskThread_t*) s_allTaskThreads;
static Atom32_t s_allTaskThreadsSpinLock;

static int __save_task_thread(TaskThread_t* t) {
	int save_ok;
	while (_xchg32(&s_allTaskThreadsSpinLock, 1));
	dynarrInsert(&s_allTaskThreads, s_allTaskThreads.len, t, save_ok);
	_xchg32(&s_allTaskThreadsSpinLock, 0);
	return save_ok;
}

static void __remove_task_thread(TaskThread_t* t) {
	size_t idx;
	while (_xchg32(&s_allTaskThreadsSpinLock, 1));
	dynarrFindValue(&s_allTaskThreads, t, idx);
	if (idx != -1) {
		dynarrRemoveIdx(&s_allTaskThreads, idx);
	}
	_xchg32(&s_allTaskThreadsSpinLock, 0);
}

/**************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

TaskThread_t* newTaskThreadStackCo(size_t co_stack_size) {
	int sche_ok = 0, seedval;
	TaskThreadStackCo_t* thrd = (TaskThreadStackCo_t*)malloc(sizeof(TaskThreadStackCo_t));
	if (!thrd) {
		return NULL;
	}

	thrd->_.sche = StackCoSche_new(co_stack_size, &thrd->_);
	if (!thrd->_.sche) {
		goto err;
	}
	sche_ok = 1;

	if (!__save_task_thread(&thrd->_)) {
		goto err;
	}
	thrd->_.entry = task_thread_stack_co_entry;
	thrd->_.exit = task_thread_stack_co_exit;
	thrd->_.deleter = task_thread_stack_co_deleter;

	seedval = time(NULL);
	mt19937Seed(&thrd->_.randmt19937_ctx, seedval);
	thrd->net_dispatch = NULL;
	return &thrd->_;
err:
	if (sche_ok) {
		StackCoSche_destroy(thrd->_.sche);
	}
	free(thrd);
	return NULL;
}

BOOL runTaskThread(TaskThread_t* t) {
	return threadCreate(&t->tid, t->entry, t);
}

void freeTaskThread(TaskThread_t* t) {
	if (t) {
		__remove_task_thread(t);
		if (t->deleter) {
			t->deleter(t);
		}
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
