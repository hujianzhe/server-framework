#include "global.h"
#include "config.h"
#include "work_thread.h"
#include <stdio.h>

#ifdef USE_STATIC_MODULE
#ifdef __cplusplus
extern "C" {
#endif
	void destroy(void);
#ifdef __cplusplus
}
#endif
#endif

static void call_dispatch(TaskThread_t* thrd, UserMsg_t* ctrl) {
	if (g_ModuleInitFunc) {
		if (!g_ModuleInitFunc(thrd, g_MainArgc, g_MainArgv)) {
			fprintf(stderr, "init(argc, argv) return failure\n");
			g_Valid = 0;
		}
		g_ModuleInitFunc = NULL;
	}
	else {
		DispatchCallback_t callback;
		if (ctrl->cmdstr) {
			callback = getStringDispatch(thrd->dispatch, ctrl->cmdstr);
		}
		else {
			callback = getNumberDispatch(thrd->dispatch, ctrl->cmdid);
		}
		if (callback)
			callback(thrd, ctrl);
		else if (thrd->dispatch->null_dispatch_callback)
			thrd->dispatch->null_dispatch_callback(thrd, ctrl);
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
	}
	ctrl->on_free(ctrl);
}

static int session_expire_timeout_callback(RBTimer_t* timer, RBTimerEvent_t* e) {
	Session_t* session = (Session_t*)e->arg;
	if (session->destroy)
		session->destroy(session);
	return 0;
}

static void rpc_fiber_msg_handler(RpcFiberCore_t* rpc, UserMsg_t* ctrl) {
	TaskThread_t* thrd = (TaskThread_t*)rpc->base.runthread;
	if (USER_MSG_EXTRA_TIMER_EVENT == ctrl->param.type) {
		RBTimerEvent_t* e = ctrl->param.timer_event;
		e->callback(&thrd->timer, e);
	}
	else {
		call_dispatch(thrd, ctrl);
	}
}

static unsigned int THREAD_CALL taskThreadEntry(void* arg) {
	ListNode_t* cur, *next;
	int wait_msec;
	long long cur_msec, timer_min_msec;
	TaskThread_t* thread = (TaskThread_t*)arg;
	RBTimer_t* due_timer[] = { &thread->timer, &thread->rpc_timer, &thread->fiber_sleep_timer };
	// init rpc
	if (g_Config.rpc_fiber) {
		Fiber_t* thread_fiber = fiberFromThread();
		if (!thread_fiber) {
			fputs("fiberFromThread error", stderr);
			g_Valid = 0;
			return 1;
		}
		thread->f_rpc = (RpcFiberCore_t*)malloc(sizeof(RpcFiberCore_t));
		if (!thread->f_rpc) {
			fputs("malloc(sizeof(RpcFiberCore_t)) error", stderr);
			g_Valid = 0;
			return 1;
		}
		if (!rpcFiberCoreInit(thread->f_rpc, thread_fiber, g_Config.rpc_fiber_stack_size,
				(void(*)(RpcFiberCore_t*, void*))rpc_fiber_msg_handler))
		{
			fputs("rpcFiberCoreInit error", stderr);
			g_Valid = 0;
			return 1;
		}
		thread->f_rpc->base.runthread = thread;
	}
	else if (g_Config.rpc_async) {
		thread->a_rpc = (RpcAsyncCore_t*)malloc(sizeof(RpcAsyncCore_t));
		if (!thread->a_rpc) {
			fputs("malloc(sizeof(RpcAsyncCore_t)) error", stderr);
			g_Valid = 0;
			return 1;
		}
		if (!rpcAsyncCoreInit(thread->a_rpc)) {
			fputs("rpcAsyncCoreInit error", stderr);
			g_Valid = 0;
			return 1;
		}
		thread->a_rpc->base.runthread = thread;
	}
	// start
	wait_msec = -1;
	while (g_Valid) {
		// handle message and event
		for (cur = dataqueuePopWait(&thread->dq, wait_msec, ~0); cur; cur = next) {
			ReactorCmd_t* internal = (ReactorCmd_t*)cur;
			next = cur->next;
			if (REACTOR_USER_CMD == internal->type) {
				UserMsg_t* ctrl = pod_container_of(internal , UserMsg_t, internal);
				if (ctrl->be_from_cluster) {
					if (RPC_STATUS_HAND_SHAKE == ctrl->rpc_status) {
						Channel_t* channel = ctrl->channel;
						ClusterNode_t* clsnd = flushClusterNodeFromJsonData(thread->clstbl, (char*)ctrl->data);
						ctrl->on_free(ctrl);
						if (clsnd) {
							if (clsnd->session.channel_server != channel)
								sessionChannelReplaceServer(&clsnd->session, channel);
							g_ConnectionNum++;
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
						if (session && session->channel_client)
							ctrl->channel = session->channel_client;
					}
				}
				if (g_Config.enqueue_timeout_msec > 0 && ctrl->enqueue_time_msec > 0) {
					cur_msec = gmtimeMillisecond();
					if (cur_msec - ctrl->enqueue_time_msec >= g_Config.enqueue_timeout_msec) {
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
					logInfo(&g_Log, "channel(%p) detach, reason:%d, connected times: %u",
						channel, channel->_.detach_error, channel->_.connected_times);
				}
				else if (channel->_.flag & CHANNEL_FLAG_SERVER) {
					logInfo(&g_Log, "channel(%p) detach, reason:%d", channel, channel->_.detach_error);
				}
				else {
					IPString_t listen_ip;
					unsigned short listen_port;
					sockaddrDecode(&channel->_.listen_addr.sa, listen_ip, &listen_port);
					logInfo(&g_Log, "listen ip(%s) port(%u) detach", listen_ip, listen_port);
				}

				if ((channel->_.flag & CHANNEL_FLAG_CLIENT) ||
					(channel->_.flag & CHANNEL_FLAG_SERVER))
				{
					if (g_ConnectionNum > 0)
						g_ConnectionNum--;

					freeRpcItemWhenChannelDetach(thread, channel);

					do {
						Session_t* session = channelSession(channel);
						if (!session)
							break;
						else if (session->channel_client == channel)
							session->channel_client = NULL;
						else if (session->channel_server == channel)
							session->channel_server = NULL;
						channelSession(channel) = NULL;
						channelSessionId(channel) = 0;
						if (session->channel_client || session->channel_server)
							break;
						if (session->disconnect)
							session->disconnect(session);
						if (session->persist)
							break;
						if (session->expire_timeout_msec > 0) {
							RBTimerEvent_t* e = (RBTimerEvent_t*)malloc(sizeof(RBTimerEvent_t));
							if (e) {
								e->arg = session;
								e->callback = session_expire_timeout_callback;
								e->timestamp_msec = gmtimeMillisecond() + session->expire_timeout_msec;
								session->expire_timeout_ev = e;
								if (rbtimerAddEvent(&thread->timer, e)) {
									break;
								}
								free(e);
							}
						}
						if (session->destroy)
							session->destroy(session);
					} while (0);
				}

				if (channel->_.memref && !memrefDecrStrong(&channel->_.memref)) {
					continue;
				}
				reactorCommitCmd(NULL, &channel->_.freecmd);
			}
		}

		// handle timer event
		cur_msec = gmtimeMillisecond();
		for (cur = rbtimerTimeout(&thread->rpc_timer, cur_msec); cur; cur = next) {
			RBTimerEvent_t* e = pod_container_of(cur, RBTimerEvent_t, m_listnode);
			RpcItem_t* rpc_item = (RpcItem_t*)e->arg;
			next = cur->next;
			rpc_item->timeout_ev = NULL;
			if (thread->f_rpc)
				rpcFiberCoreCancel(thread->f_rpc, rpc_item);
			else if (thread->a_rpc)
				rpcAsyncCoreCancel(thread->a_rpc, rpc_item);
			freeRpcItemWhenNormal(&thread->rpc_timer, rpc_item);
		}
		if (thread->f_rpc) {
			static UserMsg_t timer_msg;
			timer_msg.param.type = USER_MSG_EXTRA_TIMER_EVENT;
			for (cur = rbtimerTimeout(&thread->timer, cur_msec); cur; cur = next) {
				RBTimerEvent_t* e = pod_container_of(cur, RBTimerEvent_t, m_listnode);
				next = cur->next;
				timer_msg.param.timer_event = e;
				rpcFiberCoreResumeMsg(thread->f_rpc, &timer_msg);
			}
			for (cur = rbtimerTimeout(&thread->fiber_sleep_timer, cur_msec); cur; cur = next) {
				RBTimerEvent_t* e = pod_container_of(cur, RBTimerEvent_t, m_listnode);
				RpcItem_t* rpc_item = (RpcItem_t*)e->arg;
				next = cur->next;
				rpc_item->timeout_ev = NULL;
				rpcFiberCoreCancel(thread->f_rpc, rpc_item);
			}
		}
		else {
			for (cur = rbtimerTimeout(&thread->timer, cur_msec); cur; cur = next) {
				RBTimerEvent_t* e = pod_container_of(cur, RBTimerEvent_t, m_listnode);
				next = cur->next;
				e->callback(&thread->timer, e);
			}
		}
		if (rbtimerDueFirst(due_timer, sizeof(due_timer) / sizeof(due_timer[0]), &timer_min_msec)) {
			if (timer_min_msec > cur_msec)
				wait_msec = timer_min_msec - cur_msec;
			else
				wait_msec = 0;
		}
		else {
			wait_msec = -1;
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
	for (cur = rbtimerClean(&thread->timer); cur; cur = next) {
		next = cur->next;
		free(pod_container_of(cur, RBTimerEvent_t, m_listnode));
	}
	for (cur = dataqueueClean(&thread->dq); cur; cur = next) {
		ReactorCmd_t* internal = (ReactorCmd_t*)cur;
		next = cur->next;
		if (REACTOR_USER_CMD == internal->type) {
			UserMsg_t* ctrl = pod_container_of(internal, UserMsg_t, internal);
			ctrl->on_free(ctrl);
		}
	}
#ifdef USE_STATIC_MODULE
	destroy();
#else
	if (g_ModulePtr) {
		void(*module_destroy_fn_ptr)(void) = (void(*)(void))moduleSymbolAddress(g_ModulePtr, "destroy");
		if (module_destroy_fn_ptr)
			module_destroy_fn_ptr();
	}
#endif
	return 0;
}

/**************************************************************************************/

TaskThread_t* g_TaskThread;

#ifdef __cplusplus
extern "C" {
#endif

TaskThread_t* ptr_g_TaskThread(void) { return g_TaskThread; }

TaskThread_t* newTaskThread(void) {
	int dq_ok = 0, timer_ok = 0, rpc_timer_ok = 0, fiber_sleep_timer_ok = 0, dispatch_ok = 0;
	TaskThread_t* t = (TaskThread_t*)malloc(sizeof(TaskThread_t));
	if (!t)
		return NULL;

	if (!dataqueueInit(&t->dq))
		goto err;
	dq_ok = 1;

	if (!rbtimerInit(&t->timer, TRUE))
		goto err;
	timer_ok = 1;

	if (!rbtimerInit(&t->rpc_timer, TRUE))
		goto err;
	rpc_timer_ok = 1;

	if (!rbtimerInit(&t->fiber_sleep_timer, FALSE))
		goto err;
	fiber_sleep_timer_ok = 1;

	t->dispatch = newDispatch();
	if (!t->dispatch)
		goto err;
	dispatch_ok = 1;

	t->f_rpc = NULL;
	t->a_rpc = NULL;
	t->clstbl = NULL;
	return t;
err:
	if (dq_ok)
		dataqueueDestroy(&t->dq);
	if (timer_ok)
		rbtimerDestroy(&t->timer);
	if (rpc_timer_ok)
		rbtimerDestroy(&t->rpc_timer);
	if (fiber_sleep_timer_ok)
		rbtimerDestroy(&t->fiber_sleep_timer);
	if (dispatch_ok)
		freeDispatch(t->dispatch);
	free(t);
	return NULL;
}

BOOL runTaskThread(TaskThread_t* t) {
	return threadCreate(&t->tid, taskThreadEntry, t);
}

void freeTaskThread(TaskThread_t* t) {
	if (t) {
		if (t == g_TaskThread)
			g_TaskThread = NULL;
		dataqueueDestroy(&t->dq);
		rbtimerDestroy(&t->timer);
		rbtimerDestroy(&t->rpc_timer);
		rbtimerDestroy(&t->fiber_sleep_timer);
		freeDispatch(t->dispatch);
		free(t);
	}
}

#ifdef __cplusplus
}
#endif
