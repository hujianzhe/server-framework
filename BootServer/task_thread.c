#include "global.h"
#include "config.h"
#include "task_thread.h"
#include <stdio.h>

void TaskThread_channel_base_detach(struct StackCoSche_t* sche, void* arg) {
	TaskThread_t* thrd = (TaskThread_t*)StackCoSche_userdata(sche);
	ChannelBase_t* channel = (ChannelBase_t*)arg;
	Session_t* session = channelSession(channel);

	if (thrd->on_channel_detach) {
		thrd->on_channel_detach(thrd, channel);
	}
	if (session) {
		if (session->channel_client == channel) {
			session->channel_client = NULL;
		}
		if (session->channel_server == channel) {
			session->channel_server = NULL;
		}
		if (!sessionChannel(session)) {
			if (session->on_disconnect) {
				session->on_disconnect(thrd, session);
			}
		}
	}
	channelbaseClose(channel);
}

void TaskThread_default_clsnd_handshake(TaskThread_t* thrd, UserMsg_t* ctrl) {
	ClusterNode_t* clsnd = flushClusterNodeFromJsonData(thrd->clstbl, (char*)ctrl->data);
	if (clsnd) {
		sessionReplaceChannel(&clsnd->session, ctrl->channel);
		clsnd->status = CLSND_STATUS_NORMAL;
	}
	else {
		channelbaseSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
	}
}

static unsigned int THREAD_CALL taskThreadEntry(void* arg) {
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

	t->clstbl = NULL;
	t->errmsg = NULL;
	seedval = time(NULL);
	rand48Seed(&t->rand48_ctx, seedval);
	mt19937Seed(&t->randmt19937_ctx, seedval);
	t->filter_dispatch = NULL;
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

void TaskThread_call_dispatch(struct StackCoSche_t* sche, void* arg) {
	TaskThread_t* thrd = (TaskThread_t*)StackCoSche_userdata(sche);
	UserMsg_t* msg = (UserMsg_t*)arg;
	ChannelBase_t* c = msg->channel;

	if (!msg->retry && c) {
		channelbaseAddRef(c);
	}
	if (thrd->filter_dispatch) {
		thrd->filter_dispatch(thrd, msg);
	}
	else {
		msg->callback(thrd, msg);
	}
	if (msg->retry) {
		StackCoSche_no_arg_free(sche);
		return;
	}
	if (msg->order_dq) {
		UserMsg_t* next_msg = UserMsgExecQueue_pop(msg->order_dq);
		msg->order_dq->exec_msg = NULL;
		if (next_msg) {
			StackCoSche_function(sche, TaskThread_call_dispatch, next_msg, (void(*)(void*))freeUserMsg);
		}
	}
	if (c) {
		channelbaseClose(c);
	}
}

#ifdef __cplusplus
}
#endif
