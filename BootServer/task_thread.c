#include "global.h"
#include "config.h"
#include "task_thread.h"
#include <stdio.h>

static unsigned int task_thread_stack_co_entry(void* arg) {
	TaskThread_t* t = (TaskThread_t*)arg;
	if (t->detached) {
		threadDetach(threadSelf());
	}
	while (0 == StackCoSche_sche(t->sche_stack_co, -1));
	t->exited = 1;
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

static DynArr_t(TaskThread_t*) s_TaskThreads;
static Atom32_t s_SpinLock;

BOOL reserveTaskThreadMaxCnt(unsigned int cnt) {
	return dynarrReserve(&s_TaskThreads, cnt) != NULL;
}

void stopAllTaskThreads(void) {
	size_t i;
	while (_xchg32(&s_SpinLock, 1));
	for (i = 0; i < s_TaskThreads.len; ++i) {
		TaskThread_t* t = s_TaskThreads.buf[i];
		t->hook->exit(t);
	}
	_xchg32(&s_SpinLock, 0);
}

void waitFreeAllTaskThreads(void) {
	size_t i = 0;
	while (1) {
		while (_xchg32(&s_SpinLock, 1)) {
			threadSleepMillsecond(40);
		}
		for (; i < s_TaskThreads.len; ++i) {
			TaskThread_t* t = s_TaskThreads.buf[i];
			if (!t->exited) {
				break;
			}
		}
		if (i >= s_TaskThreads.len) {
			break;
		}
		_xchg32(&s_SpinLock, 0);
		threadSleepMillsecond(40);
	}
	for (i = 0; i < s_TaskThreads.len; ++i) {
		TaskThread_t* t = s_TaskThreads.buf[i];
		t->hook->deleter(t);
	}
	dynarrFreeMemory(&s_TaskThreads);
	_xchg32(&s_SpinLock, 0);
}

/**************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

BOOL saveTaskThread(TaskThread_t* t) {
	int save_ok;
	if (!t) {
		return 0;
	}
	save_ok = 0;
	while (_xchg32(&s_SpinLock, 1));
	if (s_TaskThreads.len <= 0) {
		dynarrReserve(&s_TaskThreads, 1);
	}
	if (s_TaskThreads.len < s_TaskThreads.capacity) {
		size_t i;
		for (i = 0; i < s_TaskThreads.len; ++i) {
			TaskThread_t* t = s_TaskThreads.buf[i];
			if (s_TaskThreads.buf[i] == t) {
				save_ok = 1;
				break;
			}
		}
		if (!save_ok) {
			s_TaskThreads.buf[s_TaskThreads.len++] = t;
			save_ok = 1;
		}
	}
	_xchg32(&s_SpinLock, 0);
	return save_ok;
}

TaskThread_t* newTaskThreadStackCo(size_t co_stack_size) {
	int sche_ok = 0;
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
	thrd->_.detached = 1;
	thrd->_.exited = 0;
	mt19937Seed(&thrd->_.randmt19937_ctx, time(NULL));
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

void stopTaskThread(TaskThread_t* t) {
	t->hook->exit(t);
}

void freeTaskThread(TaskThread_t* t) {
	if (t) {
		t->hook->deleter(t);
	}
}

TaskThread_t* currentTaskThread(void) {
	Thread_t tid = threadSelf();
	TaskThread_t* thrd = NULL;
	size_t i;
	while (_xchg32(&s_SpinLock, 1));
	for (i = 0; i < s_TaskThreads.len; ++i) {
		thrd = s_TaskThreads.buf[i];
		if (threadEqual(tid, thrd->tid)) {
			break;
		}
	}
	_xchg32(&s_SpinLock, 0);
	return thrd;
}

#ifdef __cplusplus
}
#endif
