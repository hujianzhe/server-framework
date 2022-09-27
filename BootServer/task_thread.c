#include "global.h"
#include "config.h"
#include "task_thread.h"
#include <stdio.h>

static void call_dispatch(TaskThread_t* thrd, UserMsg_t* ctrl) {
	DispatchCallback_t callback;
	Dispatch_t* dispatch = thrd->dispatch;

	if (ctrl->cmdstr) {
		callback = getStringDispatch(dispatch, ctrl->cmdstr);
	}
	else {
		callback = getNumberDispatch(dispatch, ctrl->cmdid);
	}
	if (callback) {
		if (thrd->filter_callback) {
			thrd->filter_callback(thrd, callback, ctrl);
		}
		else {
			callback(thrd, ctrl);
		}
	}
	else if (dispatch->null_dispatch_callback) {
		dispatch->null_dispatch_callback(thrd, ctrl);
	}
	else {
		if (USER_MSG_PARAM_HTTP_FRAME == ctrl->param.type) {
			if (ctrl->param.httpframe) {
				const char reply[] = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n";
				channelbaseSend(ctrl->channel, reply, sizeof(reply) - 1, NETPACKET_FRAGMENT);
				channelbaseSend(ctrl->channel, NULL, 0, NETPACKET_FIN);
			}
		}
	}
	ctrl->on_free(ctrl);
}

static void do_channel_detach(TaskThread_t* thrd, ChannelBase_t* channel) {
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
	reactorCommitCmd(NULL, &channel->freecmd);
}

static void rpc_fiber_msg_handler(RpcFiberCore_t* rpc, UserMsg_t* ctrl) {
	TaskThread_t* thrd = (TaskThread_t*)rpc->base.runthread;
	if (USER_MSG_PARAM_INIT == ctrl->param.type) {
		if (!thrd->fn_init(thrd, thrd->init_argc, thrd->init_argv)) {
			thrd->errmsg = strFormat(NULL, "task thread fn_init failure\n");
			ptrBSG()->valid = 0;
			return;
		}
	}
	else if (USER_MSG_PARAM_CHANNEL_DETACH == ctrl->param.type) {
		do_channel_detach(thrd, ctrl->channel);
	}
	else if (USER_MSG_PARAM_TIMER_EVENT == ctrl->param.type) {
		RBTimerEvent_t* e = ctrl->param.timer_event;
		e->callback(&thrd->timer, e);
	}
	else {
		ChannelBase_t* c = ctrl->channel;
		if (c) {
			channelbaseAddRef(c);
			call_dispatch(thrd, ctrl);
			reactorCommitCmd(NULL, &c->freecmd);
		}
		else {
			call_dispatch(thrd, ctrl);
		}
	}
}

static unsigned int THREAD_CALL taskThreadEntry(void* arg) {
	ListNode_t* iter_cur, *iter_next;
	TaskThread_t* thread = (TaskThread_t*)arg;
	const Config_t* conf = ptrBSG()->conf;
	Log_t* log = ptrBSG()->log;
	RpcBaseCore_t* rpc_base;
	// init rpc
	if (conf->rpc_fiber) {
		Fiber_t* thread_fiber = fiberFromThread();
		if (!thread_fiber) {
			thread->errmsg = strFormat(NULL, "task thread fiberFromThread error\n");
			ptrBSG()->valid = 0;
			return 1;
		}
		thread->f_rpc = (RpcFiberCore_t*)malloc(sizeof(RpcFiberCore_t));
		if (!thread->f_rpc) {
			thread->errmsg = strFormat(NULL, "task thread malloc(sizeof(RpcFiberCore_t)) error\n");
			ptrBSG()->valid = 0;
			return 1;
		}
		if (!rpcFiberCoreInit(thread->f_rpc, thread_fiber, conf->rpc_fiber_stack_size,
				(void(*)(RpcFiberCore_t*, void*))rpc_fiber_msg_handler))
		{
			thread->errmsg = strFormat(NULL, "task thread rpcFiberCoreInit error\n");
			ptrBSG()->valid = 0;
			return 1;
		}
		thread->f_rpc->base.runthread = thread;
		rpc_base = &thread->f_rpc->base;
	}
	else if (conf->rpc_async) {
		thread->a_rpc = (RpcAsyncCore_t*)malloc(sizeof(RpcAsyncCore_t));
		if (!thread->a_rpc) {
			thread->errmsg = strFormat(NULL, "task thread malloc(sizeof(RpcAsyncCore_t)) error\n");
			ptrBSG()->valid = 0;
			return 1;
		}
		if (!rpcAsyncCoreInit(thread->a_rpc)) {
			thread->errmsg = strFormat(NULL, "task thread rpcAsyncCoreInit error\n");
			ptrBSG()->valid = 0;
			return 1;
		}
		thread->a_rpc->base.runthread = thread;
		rpc_base = &thread->a_rpc->base;
	}
	else {
		rpc_base = NULL;
	}
	// call global init function
	if (thread->fn_init) {
		if (thread->f_rpc) {
			UserMsg_t init_msg;
			init_msg.param.type = USER_MSG_PARAM_INIT;
			rpcFiberCoreResumeMsg(thread->f_rpc, &init_msg);
		}
		else if (!thread->fn_init(thread, ptrBSG()->argc, ptrBSG()->argv)) {
			thread->errmsg = strFormat(NULL, "task thread fn_init failure\n");
			ptrBSG()->valid = 0;
			return 1;
		}
	}
	// start loop
	while (ptrBSG()->valid) {
		int i;
		long long cur_msec;
		long long t[2] = {
			rbtimerMiniumTimestamp(&thread->timer),
			rpcGetMiniumTimeoutTimestamp(rpc_base)
		};
		long long wait_msec = -1;
		for (i = 0; i < sizeof(t) / sizeof(t[0]); ++i) {
			if (t[i] < 0) {
				continue;
			}
			if (wait_msec < 0 || t[i] < wait_msec) {
				wait_msec = t[i];
			}
		}
		if (wait_msec > 0) {
			cur_msec = gmtimeMillisecond();
			if (wait_msec <= cur_msec) {
				wait_msec = 0;
			}
			else {
				wait_msec -= cur_msec;
			}
		}
		iter_cur = dataqueuePopWait(&thread->dq, wait_msec, conf->once_handle_msg_maxcnt);
		// handle message and event
		cur_msec = gmtimeMillisecond();
		for (; iter_cur; iter_cur = iter_next) {
			ReactorCmd_t* rc = pod_container_of(iter_cur, ReactorCmd_t, _);
			iter_next = iter_cur->next;
			if (REACTOR_USER_CMD == rc->type) {
				UserMsg_t* ctrl = pod_container_of(rc, UserMsg_t, internal);
				Session_t* session;
				if (ctrl->be_from_cluster) {
					if (RPC_STATUS_HAND_SHAKE == ctrl->rpc_status) {
						ChannelBase_t* channel = ctrl->channel;
						ClusterNode_t* clsnd = flushClusterNodeFromJsonData(thread->clstbl, (char*)ctrl->data);
						ctrl->on_free(ctrl);
						if (clsnd) {
							sessionReplaceChannel(&clsnd->session, channel);
							clsnd->status = CLSND_STATUS_NORMAL;
						}
						else {
							channelbaseSendv(channel, NULL, 0, NETPACKET_FIN);
						}
						continue;
					}
				}
				if (conf->enqueue_timeout_msec > 0 && ctrl->enqueue_time_msec > 0) {
					cur_msec = gmtimeMillisecond();
					if (cur_msec - ctrl->enqueue_time_msec >= conf->enqueue_timeout_msec) {
						ctrl->on_free(ctrl);
						continue;
					}
				}
				session = channelSession(ctrl->channel);
				if (session && session->channel_client) {
					ctrl->channel = session->channel_client;
				}

				if (thread->f_rpc) {
					if (RPC_STATUS_RESP == ctrl->rpc_status) {
						RpcItem_t* rpc_item = rpcFiberCoreResume(thread->f_rpc, ctrl->rpcid, ctrl);
						ctrl->on_free(ctrl);
						if (!rpc_item) {
							continue;
						}
						freeRpcItemWhenNormal(rpc_item);
					}
					else {
						rpcFiberCoreResumeMsg(thread->f_rpc, ctrl);
					}
				}
				else if (thread->a_rpc) {
					if (RPC_STATUS_RESP == ctrl->rpc_status) {
						RpcItem_t* rpc_item = rpcAsyncCoreCallback(thread->a_rpc, ctrl->rpcid, ctrl);
						ctrl->on_free(ctrl);
						if (!rpc_item) {
							continue;
						}
						freeRpcItemWhenNormal(rpc_item);
					}
					else {
						call_dispatch(thread, ctrl);
					}
				}
				else {
					call_dispatch(thread, ctrl);
				}
			}
			else if (REACTOR_CHANNEL_FREE_CMD == rc->type) {
				ChannelBase_t* channel = pod_container_of(rc, ChannelBase_t, freecmd);
				if ((channel->flag & CHANNEL_FLAG_CLIENT) ||
					(channel->flag & CHANNEL_FLAG_SERVER))
				{
					freeRpcItemWhenChannelDetach(thread, channel);
				}
				if (thread->f_rpc) {
					UserMsg_t detach_msg;
					detach_msg.param.type = USER_MSG_PARAM_CHANNEL_DETACH;
					detach_msg.channel = channel;
					rpcFiberCoreResumeMsg(thread->f_rpc, &detach_msg);
					continue;
				}
				do_channel_detach(thread, channel);
			}
		}

		// handle timer event
		cur_msec = gmtimeMillisecond();
		if (rpc_base) {
			for (i = 0; i < conf->once_rpc_timeout_items_maxcnt; ++i) {
				RpcItem_t* rpc_item = rpcGetTimeoutItem(rpc_base, cur_msec);
				if (!rpc_item) {
					break;
				}
				if (thread->f_rpc) {
					rpcFiberCoreCancel(thread->f_rpc, rpc_item);
				}
				else {
					rpcAsyncCoreCancel(thread->a_rpc, rpc_item);
				}
				freeRpcItemWhenNormal(rpc_item);
			}
		}
		if (thread->f_rpc) {
			UserMsg_t timer_msg;
			timer_msg.param.type = USER_MSG_PARAM_TIMER_EVENT;
			for (i = 0; i < conf->once_timeout_events_maxcnt; ++i) {
				RBTimerEvent_t* e = rbtimerTimeoutPopup(&thread->timer, cur_msec);
				if (!e) {
					break;
				}
				timer_msg.param.timer_event = e;
				rpcFiberCoreResumeMsg(thread->f_rpc, &timer_msg);
			}
		}
		else {
			for (i = 0; i < conf->once_timeout_events_maxcnt; ++i) {
				RBTimerEvent_t* e = rbtimerTimeoutPopup(&thread->timer, cur_msec);
				if (!e) {
					break;
				}
				e->callback(&thread->timer, e);
			}
		}
	}
	// thread exit clean
	if (thread->f_rpc) {
		rpcFiberCoreDestroy(thread->f_rpc);
		fiberFree(thread->f_rpc->sche_fiber);
		free(thread->f_rpc);
		thread->f_rpc = NULL;
	}
	else if (thread->a_rpc) {
		rpcAsyncCoreDestroy(thread->a_rpc);
		free(thread->a_rpc);
		thread->a_rpc = NULL;
	}

	if (thread->fn_destroy) {
		thread->fn_destroy(thread);
	}
	return 0;
}

/**************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

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

TaskThread_t* newTaskThread(void) {
	int dq_ok = 0, timer_ok = 0, dispatch_ok = 0, seedval = 0;
	TaskThread_t* t = (TaskThread_t*)malloc(sizeof(TaskThread_t));
	if (!t) {
		return NULL;
	}

	if (!dataqueueInit(&t->dq)) {
		goto err;
	}
	dq_ok = 1;

	if (!rbtimerInit(&t->timer)) {
		goto err;
	}
	timer_ok = 1;

	t->dispatch = newDispatch();
	if (!t->dispatch) {
		goto err;
	}
	dispatch_ok = 1;

	if (!__save_task_thread(t)) {
		goto err;
	}

	t->f_rpc = NULL;
	t->a_rpc = NULL;
	t->clstbl = NULL;
	t->init_argc = 0;
	t->init_argv = NULL;
	t->fn_init = NULL;
	t->fn_destroy = NULL;
	t->errmsg = NULL;
	seedval = time(NULL);
	rand48Seed(&t->rand48_ctx, seedval);
	mt19937Seed(&t->randmt19937_ctx, seedval);
	t->filter_callback = NULL;
	t->on_channel_detach = NULL;
	return t;
err:
	if (dq_ok) {
		dataqueueDestroy(&t->dq);
	}
	if (timer_ok) {
		rbtimerDestroy(&t->timer);
	}
	if (dispatch_ok) {
		freeDispatch(t->dispatch);
	}
	free(t);
	return NULL;
}

BOOL runTaskThread(TaskThread_t* t) {
	return threadCreate(&t->tid, taskThreadEntry, t);
}

void freeTaskThread(TaskThread_t* t) {
	ListNode_t *lcur, *lnext;
	if (t) {
		__remove_task_thread(t);
		for (lcur = dataqueueDestroy(&t->dq); lcur; lcur = lnext) {
			ReactorCmd_t* rc = pod_container_of(lcur, ReactorCmd_t, _);
			lnext = lcur->next;
			if (REACTOR_USER_CMD == rc->type) {
				UserMsg_t* ctrl = pod_container_of(rc, UserMsg_t, internal);
				ctrl->on_free(ctrl);
			}
		}
		rbtimerDestroy(&t->timer);
		freeDispatch(t->dispatch);
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

#ifdef __cplusplus
}
#endif
