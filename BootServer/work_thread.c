#include "global.h"
#include "config.h"
#include "work_thread.h"
#include <stdio.h>

static void call_dispatch(TaskThread_t* thread, UserMsg_t* ctrl) {
	if (g_ModuleInitFunc) {
		if (!g_ModuleInitFunc(thread, g_MainArgc, g_MainArgv)) {
			fprintf(stderr, "(%s).init(argc, argv) return failure\n", g_MainArgv[1]);
			g_Valid = 0;
		}
		g_ModuleInitFunc = NULL;
	}
	else {
		DispatchCallback_t callback;
		if (ctrl->cmdstr) {
			callback = getStringDispatch(ctrl->cmdstr);
		}
		else {
			callback = getNumberDispatch(ctrl->cmdid);
		}
		if (callback)
			callback(thread, ctrl);
		else if (g_DefaultDispatchCallback)
			g_DefaultDispatchCallback(thread, ctrl);
		else {
			if (ctrl->httpframe) {
				char reply[] = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n";
				channelSend(ctrl->channel, reply, sizeof(reply) - 1, NETPACKET_FRAGMENT);
				reactorCommitCmd(NULL, &ctrl->channel->_.stream_sendfincmd);
			}
			else {
				SendMsg_t ret_msg;
				if (ctrl->rpc_status == 'R') {
					makeSendMsgRpcResp(&ret_msg, ctrl->rpcid, 0, NULL, 0);
				}
				else {
					makeSendMsg(&ret_msg, 0, NULL, 0);
				}
				channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
			}
		}
	}
	free(ctrl);
}

static int session_expire_timeout_callback(RBTimer_t* timer, RBTimerEvent_t* e) {
	Session_t* session = (Session_t*)e->arg;
	if (session->destroy)
		session->destroy(session);
	return 0;
}

static void rpc_fiber_msg_handler(RpcFiberCore_t* rpc, UserMsg_t* ctrl) {
	TaskThread_t* thread = (TaskThread_t*)rpc->runthread;
	call_dispatch(thread, ctrl);
}

static unsigned int THREAD_CALL taskThreadEntry(void* arg) {
	ListNode_t* cur, *next;
	int wait_msec;
	long long cur_msec, timer_min_msec[2];
	TaskThread_t* thread = (TaskThread_t*)arg;
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
		thread->f_rpc->runthread = thread;
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
				if (thread->f_rpc) {
					if ('T' == ctrl->rpc_status) {
						RpcItem_t* rpc_item = rpcFiberCoreResume(thread->f_rpc, ctrl->rpcid, ctrl);
						free(ctrl);
						if (!rpc_item) {
							continue;
						}
						freeRpcItemWhenNormal(thread, ctrl->channel, rpc_item);
					}
					else {
						rpcFiberCoreResumeMsg(thread->f_rpc, ctrl);
					}
				}
				else if (thread->a_rpc) {
					if ('T' == ctrl->rpc_status) {
						RpcItem_t* rpc_item = rpcAsyncCoreCallback(thread->a_rpc, ctrl->rpcid, ctrl);
						free(ctrl);
						if (!rpc_item) {
							continue;
						}
						freeRpcItemWhenNormal(thread, ctrl->channel, rpc_item);
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
				else {
					logInfo(&g_Log, "channel(%p) detach, reason:%d", channel, channel->_.detach_error);
				}

				freeRpcItemWhenChannelDetach(thread, channel);

				do {
					Session_t* session = (Session_t*)channelSession(channel);
					if (!session)
						break;
					sessionUnbindChannel(session, channel);
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

				channelDestroy(channel);
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
			freeRpcItemWhenTimeout(thread, rpc_item);
		}
		for (cur = rbtimerTimeout(&thread->timer, cur_msec); cur; cur = next) {
			RBTimerEvent_t* e = pod_container_of(cur, RBTimerEvent_t, m_listnode);
			next = cur->next;
			e->callback(&thread->timer, e);
		}
		timer_min_msec[0] = rbtimerMiniumTimestamp(&thread->timer);
		timer_min_msec[1] = rbtimerMiniumTimestamp(&thread->rpc_timer);
		if (timer_min_msec[0] < 0 && timer_min_msec[1] < 0) {
			wait_msec = -1;
		}
		else {
			long long min_msec;
			cur_msec = gmtimeMillisecond();
			if (timer_min_msec[1] < 0)
				min_msec = timer_min_msec[0];
			else if (timer_min_msec[0] < 0)
				min_msec = timer_min_msec[1];
			else if (timer_min_msec[0] < timer_min_msec[1])
				min_msec = timer_min_msec[0];
			else
				min_msec = timer_min_msec[1];
			if (min_msec > cur_msec)
				wait_msec = min_msec - cur_msec;
			else
				wait_msec = 0;
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
		free(pod_container_of(cur, RBTimerEvent_t, m_listnode));
		next = cur->next;
	}
	for (cur = dataqueueClean(&thread->dq); cur; cur = next) {
		ReactorCmd_t* internal = (ReactorCmd_t*)cur;
		next = cur->next;
		if (REACTOR_USER_CMD == internal->type)
			free(pod_container_of(internal, UserMsg_t, internal));
	}
	if (g_ModulePtr) {
		void(*module_destroy_fn_ptr)(void) = (void(*)(void))moduleSymbolAddress(g_ModulePtr, "destroy");
		if (module_destroy_fn_ptr)
			module_destroy_fn_ptr();
	}
	return 0;
}

unsigned int THREAD_CALL reactorThreadEntry(void* arg) {
	Reactor_t* reactor = (Reactor_t*)arg;
	NioEv_t e[4096];
	int wait_sec = 1000;
	while (g_Valid) {
		int n = reactorHandle(reactor, e, sizeof(e)/sizeof(e[0]), gmtimeMillisecond(), wait_sec);
		if (n < 0) {
			logErr(&g_Log, "reactorHandle error:%d", errnoGet());
			break;
		}
	}
	return 0;
}

/**************************************************************************************/

TaskThread_t* g_TaskThread;

#ifdef __cplusplus
extern "C" {
#endif

TaskThread_t* ptr_g_TaskThread(void) { return g_TaskThread; }

TaskThread_t* newTaskThread(void) {
	int dq_ok = 0, timer_ok = 0, rpc_timer_ok = 0;
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

	t->f_rpc = NULL;
	t->a_rpc = NULL;
	return t;
err:
	if (dq_ok)
		dataqueueDestroy(&t->dq);
	if (timer_ok)
		rbtimerDestroy(&t->timer);
	if (rpc_timer_ok)
		rbtimerDestroy(&t->rpc_timer);
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
		free(t);
	}
}

#ifdef __cplusplus
}
#endif