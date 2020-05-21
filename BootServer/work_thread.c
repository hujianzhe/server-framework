#include "global.h"
#include "config.h"
#include <stdio.h>

static void call_dispatch(UserMsg_t* ctrl) {
	if (g_ModuleInitFunc) {
		if (!g_ModuleInitFunc(g_MainArgc, g_MainArgv)) {
			printf("(%s).init(argc, argv) return failure\n", g_MainArgv[1]);
			g_Valid = 0;
		}
		g_ModuleInitFunc = NULL;
	}
	else {
		DispatchCallback_t callback;
		if (ctrl->httpframe) {
			char* path = ctrl->httpframe->uri;
			path[ctrl->httpframe->pathlen] = 0;
			callback = getStringDispatch(path);
		}
		else {
			callback = getNumberDispatch(ctrl->cmdid);
		}
		if (callback)
			callback(ctrl);
		else if (g_DefaultDispatchCallback)
			g_DefaultDispatchCallback(ctrl);
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

static int session_expire_timeout_callback(RBTimerEvent_t* e, void* arg) {
	Session_t* session = (Session_t*)arg;
	if (session->destroy)
		session->destroy(session);
	return 0;
}

static void msg_handler(RpcFiberCore_t* rpc, UserMsg_t* ctrl) {
	call_dispatch(ctrl);
}

unsigned int THREAD_CALL taskThreadEntry(void* arg) {
	ListNode_t* cur, *next;
	int wait_msec;
	long long cur_msec, timer_min_msec[2];
	// init rpc
	if (g_Config.rpc_fiber) {
		Fiber_t* thread_fiber = fiberFromThread();
		if (!thread_fiber) {
			fputs("fiberFromThread error", stderr);
			g_Valid = 0;
			return 1;
		}
		g_RpcFiberCore = (RpcFiberCore_t*)malloc(sizeof(RpcFiberCore_t));
		if (!g_RpcFiberCore) {
			fputs("malloc(sizeof(RpcFiberCore_t)) error", stderr);
			g_Valid = 0;
			return 1;
		}
		if (!rpcFiberCoreInit(g_RpcFiberCore, thread_fiber, g_Config.rpc_fiber_stack_size, (void(*)(RpcFiberCore_t*, void*))msg_handler)) {
			fputs("rpcFiberCoreInit error", stderr);
			g_Valid = 0;
			return 1;
		}
	}
	else if (g_Config.rpc_async) {
		g_RpcAsyncCore = (RpcAsyncCore_t*)malloc(sizeof(RpcAsyncCore_t));
		if (!g_RpcAsyncCore) {
			fputs("malloc(sizeof(RpcAsyncCore_t)) error", stderr);
			g_Valid = 0;
			return 1;
		}
		if (!rpcAsyncCoreInit(g_RpcAsyncCore)) {
			fputs("rpcAsyncCoreInit error", stderr);
			g_Valid = 0;
			return 1;
		}
	}
	// start
	wait_msec = -1;
	while (g_Valid) {
		// handle message and event
		for (cur = dataqueuePopWait(&g_DataQueue, wait_msec, ~0); cur; cur = next) {
			ReactorCmd_t* internal = (ReactorCmd_t*)cur;
			next = cur->next;
			if (REACTOR_USER_CMD == internal->type) {
				UserMsg_t* ctrl = pod_container_of(internal , UserMsg_t, internal);
				if (g_RpcFiberCore) {
					if ('T' == ctrl->rpc_status) {
						RpcItem_t* rpc_item = rpcFiberCoreResume(g_RpcFiberCore, ctrl->rpcid, ctrl);
						free(ctrl);
						if (!rpc_item) {
							continue;
						}
						freeRpcItemWhenNormal(ctrl->channel, rpc_item);
					}
					else {
						rpcFiberCoreResumeMsg(g_RpcFiberCore, ctrl);
					}
				}
				else if (g_RpcAsyncCore) {
					if ('T' == ctrl->rpc_status) {
						RpcItem_t* rpc_item = rpcAsyncCoreCallback(g_RpcAsyncCore, ctrl->rpcid, ctrl);
						free(ctrl);
						if (!rpc_item) {
							continue;
						}
						freeRpcItemWhenNormal(ctrl->channel, rpc_item);
					}
					else {
						call_dispatch(ctrl);
					}
				}
				else {
					call_dispatch(ctrl);
				}
			}
			else if (REACTOR_CHANNEL_FREE_CMD == internal->type) {
				Channel_t* channel = pod_container_of(internal, Channel_t, _.freecmd);

				printf("channel(%p) detach, reason:%d", channel, channel->_.detach_error);
				if (channel->_.flag & CHANNEL_FLAG_CLIENT)
					printf(", connected times: %u\n", channel->_.connected_times);
				else
					putchar('\n');

				freeRpcItemWhenChannelDetach(channel);

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
							if (rbtimerAddEvent(&g_Timer, e)) {
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
		for (cur = rbtimerTimeout(&g_TimerRpcTimeout, cur_msec); cur; cur = next) {
			RBTimerEvent_t* e = pod_container_of(cur, RBTimerEvent_t, m_listnode);
			RpcItem_t* rpc_item = (RpcItem_t*)e->arg;
			next = cur->next;
			rpc_item->timeout_ev = NULL;
			if (g_RpcFiberCore)
				rpcFiberCoreCancel(g_RpcFiberCore, rpc_item);
			else if (g_RpcAsyncCore)
				rpcAsyncCoreCancel(g_RpcAsyncCore, rpc_item);
			freeRpcItemWhenTimeout(rpc_item);
		}
		for (cur = rbtimerTimeout(&g_Timer, cur_msec); cur; cur = next) {
			RBTimerEvent_t* e = pod_container_of(cur, RBTimerEvent_t, m_listnode);
			next = cur->next;
			e->callback(e, e->arg);
		}
		timer_min_msec[0] = rbtimerMiniumTimestamp(&g_Timer);
		timer_min_msec[1] = rbtimerMiniumTimestamp(&g_TimerRpcTimeout);
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
	if (g_RpcFiberCore) {
		rpcFiberCoreDestroy(g_RpcFiberCore);
		fiberFree(g_RpcFiberCore->sche_fiber);
	}
	else if (g_RpcAsyncCore) {
		rpcAsyncCoreDestroy(g_RpcAsyncCore);
	}
	for (cur = rbtimerClean(&g_Timer); cur; cur = next) {
		free(pod_container_of(cur, RBTimerEvent_t, m_listnode));
		next = cur->next;
	}
	for (cur = dataqueueClean(&g_DataQueue); cur; cur = next) {
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
			printf("reactorHandle error:%d\n", errnoGet());
			break;
		}
	}
	return 0;
}
