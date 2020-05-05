#include "global.h"
#include "config.h"
#include <stdio.h>

static void call_dispatch(UserMsg_t* ctrl) {
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
	else
		g_DefaultDispatchCallback(ctrl);
	free(ctrl);
}

static int session_expire_timeout_callback(RBTimerEvent_t* e, void* arg) {
	Session_t* session = (Session_t*)arg;
	if (g_SessionAction.unreg)
		g_SessionAction.unreg(session);
	g_SessionAction.destroy(session);
	free(e);
	return 0;
}

static void msg_handler(RpcFiberCore_t* rpc, UserMsg_t* ctrl) {
	call_dispatch(ctrl);
}

unsigned int THREAD_CALL taskThreadEntry(void* arg) {
	ListNode_t* cur, *next;
	int wait_msec;
	long long cur_msec, timer_min_msec[2];
	// init
	if (g_Config.rpc_fiber) {
		Fiber_t* thread_fiber = fiberFromThread();
		if (!thread_fiber) {
			fputs("fiberFromThread error", stderr);
			return 1;
		}
		g_RpcFiberCore = (RpcFiberCore_t*)malloc(sizeof(RpcFiberCore_t));
		if (!g_RpcFiberCore) {
			fputs("malloc(sizeof(RpcFiberCore_t)) error", stderr);
			return 1;
		}
		if (!rpcFiberCoreInit(g_RpcFiberCore, thread_fiber, 0x4000, (void(*)(RpcFiberCore_t*, void*))msg_handler)) {
			fputs("rpcFiberCoreInit error", stderr);
			return 1;
		}
	}
	else if (g_Config.rpc_async) {
		g_RpcAsyncCore = (RpcAsyncCore_t*)malloc(sizeof(RpcAsyncCore_t));
		if (!g_RpcAsyncCore) {
			fputs("malloc(sizeof(RpcAsyncCore_t)) error", stderr);
			return 1;
		}
		if (!rpcAsyncCoreInit(g_RpcAsyncCore)) {
			fputs("rpcAsyncCoreInit error", stderr);
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
				Session_t* session = (Session_t*)channelSession(ctrl->channel);
				if (!session) {
					session = g_SessionAction.create(ctrl->channel->usertype);
					if (!session) {
						channelSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
						free(ctrl);
						continue;
					}
					sessionBindChannel(session, ctrl->channel);
				}

				if (g_RpcFiberCore) {
					if ('T' == ctrl->rpc_status) {
						RpcItem_t* rpc_item = rpcFiberCoreResume(g_RpcFiberCore, ctrl->rpcid, ctrl);
						free(ctrl);
						if (!rpc_item) {
							continue;
						}
						freeRpcItemWhenNormal(session, rpc_item);
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
						freeRpcItemWhenNormal(session, rpc_item);
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
				Session_t* session = (Session_t*)channelSession(channel);

				printf("channel(%p) detach, reason:%d", channel, channel->_.detach_error);
				if (channel->_.flag & CHANNEL_FLAG_CLIENT)
					printf(", connected times: %u\n", channel->_.connected_times);
				else
					putchar('\n');

				if (session) {
					ListNode_t* cur, *next;
					for (cur = session->rpc_itemlist.head; cur; cur = next) {
						RpcItem_t* rpc_item = pod_container_of(cur, RpcItem_t, listnode);
						next = cur->next;

						if (rpc_item->timeout_ev) {
							rbtimerDelEvent(&g_TimerRpcTimeout, (RBTimerEvent_t*)rpc_item->timeout_ev);
							rpc_item->timeout_ev = NULL;
						}
						rpc_item->originator = NULL;

						if (g_RpcFiberCore)
							rpcFiberCoreCancel(g_RpcFiberCore, rpc_item);
						else if (g_RpcAsyncCore)
							rpcAsyncCoreCancel(g_RpcAsyncCore, rpc_item);

						freeRpcItem(rpc_item);
					}
					listInit(&session->rpc_itemlist);
					sessionUnbindChannel(session);
					do {
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
						if (g_SessionAction.unreg)
							g_SessionAction.unreg(session);
						g_SessionAction.destroy(session);
					} while (0);
				}
				channelDestroy(channel);
				reactorCommitCmd(channel->_.reactor, &channel->_.freecmd);
			}
			else {
				printf("unknown message type: %d\n", internal->type);
			}
		}

		// handle timer event
		cur_msec = gmtimeMillisecond();
		for (cur = rbtimerTimeout(&g_TimerRpcTimeout, cur_msec); cur; cur = next) {
			RBTimerEvent_t* e = pod_container_of(cur, RBTimerEvent_t, m_listnode);
			RpcItem_t* rpc_item = (RpcItem_t*)e->arg;
			Session_t* session = (Session_t*)rpc_item->originator;
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
