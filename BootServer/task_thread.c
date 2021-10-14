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
		callback(thrd, ctrl);
	}
	else if (dispatch->null_dispatch_callback) {
		dispatch->null_dispatch_callback(thrd, ctrl);
	}
	else {
		if (USER_MSG_EXTRA_HTTP_FRAME == ctrl->param.type) {
			if (ctrl->param.httpframe) {
				const char reply[] = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n";
				channelSend(ctrl->channel, reply, sizeof(reply) - 1, NETPACKET_FRAGMENT);
				channelSend(ctrl->channel, NULL, 0, NETPACKET_FIN);
			}
		}
		else {
			InnerMsg_t ret_msg;
			if (RPC_STATUS_REQ == ctrl->rpc_status) {
				makeInnerMsgRpcResp(&ret_msg, ctrl->rpcid, 0, NULL, 0);
			}
			else {
				makeInnerMsg(&ret_msg, 0, NULL, 0);
			}
			channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
		}
	}
	ctrl->on_free(ctrl);
}

static void rpc_fiber_msg_handler(RpcFiberCore_t* rpc, UserMsg_t* ctrl) {
	TaskThread_t* thrd = (TaskThread_t*)rpc->base.runthread;
	if (thrd->__fn_init_fiber_msg == ctrl) {
		thrd->__fn_init_fiber_msg = NULL;
		if (!thrd->fn_init(thrd, thrd->init_argc, thrd->init_argv)) {
			thrd->errmsg = strFormat(NULL, "task thread fn_init failure\n");
			ptrBSG()->valid = 0;
			return;
		}
	}
	else if (USER_MSG_EXTRA_TIMER_EVENT == ctrl->param.type) {
		RBTimerEvent_t* e = ctrl->param.timer_event;
		e->callback(&thrd->timer, e);
	}
	else {
		Channel_t* c = ctrl->channel;
		if (c) {
			channelbaseAddRef(&c->_);
			call_dispatch(thrd, ctrl);
			reactorCommitCmd(NULL, &c->_.freecmd);
		}
		else {
			call_dispatch(thrd, ctrl);
		}
	}
}

static unsigned int THREAD_CALL taskThreadEntry(void* arg) {
	ListNode_t* iter_cur, *iter_next;
	int wait_msec;
	long long cur_msec, timer_min_msec;
	time_t cur_sec;
	TaskThread_t* thread = (TaskThread_t*)arg;
	RBTimer_t* due_timer[] = { &thread->timer, &thread->rpc_timer, &thread->fiber_sleep_timer };
	Config_t* conf = ptrBSG()->conf;
	Log_t* log = ptrBSG()->log;
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
	}
	// call global init function
	if (thread->fn_init) {
		if (thread->f_rpc) {
			UserMsg_t fn_init_fiber_msg;
			thread->__fn_init_fiber_msg = &fn_init_fiber_msg;
			rpcFiberCoreResumeMsg(thread->f_rpc, &fn_init_fiber_msg);
		}
		else if (!thread->fn_init(thread, ptrBSG()->argc, ptrBSG()->argv)) {
			thread->errmsg = strFormat(NULL, "task thread fn_init failure\n");
			ptrBSG()->valid = 0;
			return 1;
		}
	}
	// start loop
	while (ptrBSG()->valid) {
		if (rbtimerDueFirst(due_timer, sizeof(due_timer) / sizeof(due_timer[0]), &timer_min_msec)) {
			cur_msec = gmtimeMillisecond();
			if (timer_min_msec > cur_msec) {
				wait_msec = timer_min_msec - cur_msec;
			}
			else {
				wait_msec = 0;
			}
		}
		else {
			wait_msec = -1;
		}
		iter_cur = dataqueuePopWait(&thread->dq, wait_msec, ~0);
		// handle message and event
		cur_msec = gmtimeMillisecond();
		cur_sec = cur_msec / 1000;
		for (; iter_cur; iter_cur = iter_next) {
			ReactorCmd_t* internal = (ReactorCmd_t*)iter_cur;
			iter_next = iter_cur->next;
			if (REACTOR_USER_CMD == internal->type) {
				UserMsg_t* ctrl = pod_container_of(internal , UserMsg_t, internal);
				if (ctrl->be_from_cluster) {
					if (RPC_STATUS_HAND_SHAKE == ctrl->rpc_status) {
						Channel_t* channel = ctrl->channel;
						ClusterNode_t* clsnd = flushClusterNodeFromJsonData(thread->clstbl, (char*)ctrl->data);
						ctrl->on_free(ctrl);
						if (clsnd) {
							sessionReplaceChannel(&clsnd->session, channel);
						}
						else {
							channelSendv(channel, NULL, 0, NETPACKET_FIN);
						}
						continue;
					}
					else if (RPC_STATUS_FLUSH_NODE == ctrl->rpc_status) {
						flushClusterNodeFromJsonData(thread->clstbl, (char*)ctrl->data);
						ctrl->on_free(ctrl);
						continue;
					}
					else {
						Session_t* session = channelSession(ctrl->channel);
						if (session && session->channel_client) {
							ctrl->channel = session->channel_client;
						}
					}
				}
				if (conf->enqueue_timeout_msec > 0 && ctrl->enqueue_time_msec > 0) {
					cur_msec = gmtimeMillisecond();
					if (cur_msec - ctrl->enqueue_time_msec >= conf->enqueue_timeout_msec) {
						ctrl->on_free(ctrl);
						continue;
					}
				}
				if (thread->f_rpc) {
					if (RPC_STATUS_RESP == ctrl->rpc_status) {
						RpcItem_t* rpc_item = rpcFiberCoreResume(thread->f_rpc, ctrl->rpcid, ctrl);
						ctrl->on_free(ctrl);
						if (!rpc_item) {
							continue;
						}
						freeRpcItemWhenNormal(&thread->rpc_timer, rpc_item);
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
						freeRpcItemWhenNormal(&thread->rpc_timer, rpc_item);
					}
					else {
						call_dispatch(thread, ctrl);
					}
				}
				else {
					call_dispatch(thread, ctrl);
				}
			}
			else if (REACTOR_CHANNEL_FREE_CMD == internal->type) {
				Channel_t* channel = pod_container_of(internal, Channel_t, _.freecmd);
				if (channel->_.flag & CHANNEL_FLAG_CLIENT) {
					logInfo(log, "channel(%p) detach, reason:%d, connected times: %u",
						channel, channel->_.detach_error, channel->_.connected_times);
				}
				else if (channel->_.flag & CHANNEL_FLAG_SERVER) {
					logInfo(log, "channel(%p) detach, reason:%d", channel, channel->_.detach_error);
				}
				else {
					IPString_t listen_ip;
					unsigned short listen_port;
					sockaddrDecode(&channel->_.listen_addr.sa, listen_ip, &listen_port);
					logInfo(log, "listen ip(%s) port(%u) detach", listen_ip, listen_port);
				}

				if ((channel->_.flag & CHANNEL_FLAG_CLIENT) ||
					(channel->_.flag & CHANNEL_FLAG_SERVER))
				{
					Session_t* session = channelSession(channel);
					if (session) {
						if (session->channel_client == channel) {
							session->channel_client = NULL;
						}
						if (session->channel_server == channel) {
							session->channel_server = NULL;
						}
					}
					freeRpcItemWhenChannelDetach(thread, channel);
					if (session && !sessionChannel(session)) {
						session->reconnect_timestamp_sec = cur_sec + session->reconnect_delay_sec;
						if (session->disconnect) {
							session->disconnect(session);
						}
					}
				}
				reactorCommitCmd(NULL, &channel->_.freecmd);
			}
		}

		// handle timer event
		cur_msec = gmtimeMillisecond();
		for (iter_cur = rbtimerTimeout(&thread->rpc_timer, cur_msec); iter_cur; iter_cur = iter_next) {
			RBTimerEvent_t* e = pod_container_of(iter_cur, RBTimerEvent_t, m_listnode);
			RpcItem_t* rpc_item = (RpcItem_t*)e->arg;
			iter_next = iter_cur->next;
			rpc_item->timeout_ev = NULL;
			if (thread->f_rpc) {
				rpcFiberCoreCancel(thread->f_rpc, rpc_item);
			}
			else if (thread->a_rpc) {
				rpcAsyncCoreCancel(thread->a_rpc, rpc_item);
			}
			freeRpcItemWhenNormal(&thread->rpc_timer, rpc_item);
		}
		if (thread->f_rpc) {
			UserMsg_t timer_msg;
			timer_msg.param.type = USER_MSG_EXTRA_TIMER_EVENT;
			for (iter_cur = rbtimerTimeout(&thread->timer, cur_msec); iter_cur; iter_cur = iter_next) {
				RBTimerEvent_t* e = pod_container_of(iter_cur, RBTimerEvent_t, m_listnode);
				iter_next = iter_cur->next;
				timer_msg.param.timer_event = e;
				rpcFiberCoreResumeMsg(thread->f_rpc, &timer_msg);
			}
			for (iter_cur = rbtimerTimeout(&thread->fiber_sleep_timer, cur_msec); iter_cur; iter_cur = iter_next) {
				RBTimerEvent_t* e = pod_container_of(iter_cur, RBTimerEvent_t, m_listnode);
				RpcItem_t* rpc_item = (RpcItem_t*)e->arg;
				iter_next = iter_cur->next;
				rpc_item->timeout_ev = NULL;
				rpcFiberCoreCancel(thread->f_rpc, rpc_item);
			}
		}
		else {
			for (iter_cur = rbtimerTimeout(&thread->timer, cur_msec); iter_cur; iter_cur = iter_next) {
				RBTimerEvent_t* e = pod_container_of(iter_cur, RBTimerEvent_t, m_listnode);
				iter_next = iter_cur->next;
				e->callback(&thread->timer, e);
			}
		}
	}
	// thread exit clean
	if (thread->f_rpc) {
		rpcFiberCoreDestroy(thread->f_rpc);
		fiberFree(thread->f_rpc->sche_fiber);
	}
	else if (thread->a_rpc) {
		rpcAsyncCoreDestroy(thread->a_rpc);
	}
	for (iter_cur = rbtimerClean(&thread->timer); iter_cur; iter_cur = iter_next) {
		iter_next = iter_cur->next;
		free(pod_container_of(iter_cur, RBTimerEvent_t, m_listnode));
	}
	for (iter_cur = dataqueueClean(&thread->dq); iter_cur; iter_cur = iter_next) {
		ReactorCmd_t* internal = (ReactorCmd_t*)iter_cur;
		iter_next = iter_cur->next;
		if (REACTOR_USER_CMD == internal->type) {
			UserMsg_t* ctrl = pod_container_of(internal, UserMsg_t, internal);
			ctrl->on_free(ctrl);
		}
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

TaskThread_t* newTaskThread(void) {
	int dq_ok = 0, timer_ok = 0, rpc_timer_ok = 0, fiber_sleep_timer_ok = 0, dispatch_ok = 0;
	TaskThread_t* t = (TaskThread_t*)malloc(sizeof(TaskThread_t));
	if (!t) {
		return NULL;
	}

	if (!dataqueueInit(&t->dq)) {
		goto err;
	}
	dq_ok = 1;

	if (!rbtimerInit(&t->timer, TRUE)) {
		goto err;
	}
	timer_ok = 1;

	if (!rbtimerInit(&t->rpc_timer, TRUE)) {
		goto err;
	}
	rpc_timer_ok = 1;

	if (!rbtimerInit(&t->fiber_sleep_timer, FALSE)) {
		goto err;
	}
	fiber_sleep_timer_ok = 1;

	t->dispatch = newDispatch();
	if (!t->dispatch) {
		goto err;
	}
	dispatch_ok = 1;

	t->f_rpc = NULL;
	t->a_rpc = NULL;
	t->clstbl = NULL;
	t->init_argc = 0;
	t->init_argv = NULL;
	t->fn_init = NULL;
	t->fn_destroy = NULL;
	t->__fn_init_fiber_msg = NULL;
	t->errmsg = NULL;
	return t;
err:
	if (dq_ok) {
		dataqueueDestroy(&t->dq);
	}
	if (timer_ok) {
		rbtimerDestroy(&t->timer);
	}
	if (rpc_timer_ok) {
		rbtimerDestroy(&t->rpc_timer);
	}
	if (fiber_sleep_timer_ok) {
		rbtimerDestroy(&t->fiber_sleep_timer);
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
	if (t) {
		dataqueueDestroy(&t->dq);
		rbtimerDestroy(&t->timer);
		rbtimerDestroy(&t->rpc_timer);
		rbtimerDestroy(&t->fiber_sleep_timer);
		freeDispatch(t->dispatch);
		free((void*)t->errmsg);
		free(t);
	}
}

#ifdef __cplusplus
}
#endif
