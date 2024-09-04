#include "global.h"
#include "config.h"
#include "task_thread.h"
#include <stdio.h>

static unsigned int taskThreadEntry(void* arg) {
	TaskThread_t* thrd = (TaskThread_t*)arg;

	while (0 == StackCoSche_sche(thrd->sche, -1));

	return 0;
}

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

TaskThread_t* newTaskThread(size_t co_stack_size) {
	int sche_ok = 0, seedval;
	TaskThread_t* t = (TaskThread_t*)malloc(sizeof(TaskThread_t));
	if (!t) {
		return NULL;
	}

	t->sche = StackCoSche_new(co_stack_size, t);
	if (!t->sche) {
		goto err;
	}
	sche_ok = 1;

	if (!__save_task_thread(t)) {
		goto err;
	}

	t->errmsg = NULL;
	seedval = time(NULL);
	rand48Seed(&t->rand48_ctx, seedval);
	mt19937Seed(&t->randmt19937_ctx, seedval);
	t->net_dispatch = NULL;
	t->on_channel_detach = NULL;
	return t;
err:
	if (sche_ok) {
		StackCoSche_destroy(t->sche);
	}
	free(t);
	return NULL;
}

BOOL runTaskThread(TaskThread_t* t) {
	return threadCreate(&t->tid, taskThreadEntry, t);
}

void freeTaskThread(TaskThread_t* t) {
	if (t) {
		__remove_task_thread(t);
		StackCoSche_destroy(t->sche);
		free((void*)t->errmsg);
		free(t);
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

void TaskThread_channel_base_detach(TaskThread_t* thrd, ChannelBase_t* channel) {
	Session_t* session = channel->session;
	if (thrd->on_channel_detach) {
		thrd->on_channel_detach(thrd, channel);
	}
	if (session) {
		channel->session = NULL;
		session->channel = NULL;
		if (session->on_disconnect) {
			session->on_disconnect(session);
		}
	}
	channelbaseCloseRef(channel);
}

void TaskThread_net_dispatch(TaskThread_t* thrd, DispatchNetMsg_t* net_msg) {
#ifndef NDEBUG
	assert(thrd->net_dispatch);
#endif
	channelbaseAddRef(net_msg->channel);
	thrd->net_dispatch(thrd, net_msg);
	channelbaseCloseRef(net_msg->channel);
	net_msg->channel = NULL;
}

#ifdef __cplusplus
}
#endif
