#include "global.h"
#include "config.h"
#include <stdio.h>

static void msg_handler(RpcFiberCore_t* rpc, UserMsg_t* ctrl) {
	Session_t* session = (Session_t*)channelSession(ctrl->channel);
	DispatchCallback_t callback = getDispatchCallback(ctrl->cmdid);
	if (callback)
		callback(ctrl);
	free(ctrl);
}

unsigned int THREAD_CALL taskThreadEntry(void* arg) {
	ListNode_t* cur, *next;
	int wait_msec;
	long long cur_msec, timer_min_msec;
	Fiber_t* thread_fiber = NULL;
	if (g_Config.rpc_fiber) {
		thread_fiber = fiberFromThread();
		if (!thread_fiber) {
			fputs("fiberFromThread error", stderr);
			return 1;
		}
	}
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
					session = newSession();
					if (!session) {
						channelShardSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
						free(ctrl);
						continue;
					}
					if (g_Config.rpc_fiber) {
						session->f_rpc = (RpcFiberCore_t*)malloc(sizeof(RpcFiberCore_t));
						if (!session->f_rpc) {
							channelShardSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
							free(ctrl);
							freeSession(session);
							continue;
						}
						if (!rpcFiberCoreInit(session->f_rpc, thread_fiber, 0x4000)) {
							channelShardSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
							free(ctrl);
							free(session->f_rpc);
							freeSession(session);
							continue;
						}
						session->f_rpc->msg_handler = (void(*)(RpcFiberCore_t*, void*))msg_handler;
					}
					else if (g_Config.rpc_async) {
						session->a_rpc = (RpcAsyncCore_t*)malloc(sizeof(RpcAsyncCore_t));
						if (!session->a_rpc) {
							channelShardSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
							free(ctrl);
							freeSession(session);
							continue;
						}
						rpcAsyncCoreInit(session->a_rpc);
					}
					sessionBindChannel(session, ctrl->channel);
				}

				if (session->f_rpc) {
					if ('T' == ctrl->rpc_status) {
						RpcItem_t* rpc_item = rpcFiberCoreResume(session->f_rpc, ctrl->rpcid, ctrl);
						free(ctrl);
						if (!rpc_item) {
							continue;
						}
						free(rpc_item);
					}
					else {
						rpcFiberCoreResumeMsg(session->f_rpc, ctrl);
					}
				}
				else if (session->a_rpc) {
					if ('T' == ctrl->rpc_status) {
						RpcItem_t* rpc_item = rpcAsyncCoreCallback(session->a_rpc, ctrl->rpcid, ctrl);
						free(ctrl);
						if (!rpc_item) {
							continue;
						}
						free(rpc_item);
					}
					else {
						DispatchCallback_t callback = getDispatchCallback(ctrl->cmdid);
						if (callback)
							callback(ctrl);
						free(ctrl);
					}
				}
				else {
					DispatchCallback_t callback = getDispatchCallback(ctrl->cmdid);
					if (callback)
						callback(ctrl);
					free(ctrl);
				}
			}
			else if (REACTOR_CHANNEL_FREE_CMD == internal->type) {
				Channel_t* channel = pod_container_of(internal, Channel_t, _.freecmd);
				Session_t* session = (Session_t*)channelSession(channel);

				sessionUnbindChannel(session);

				printf("channel(%p) detach, reason:%d", channel, channel->_.detach_error);
				if (channel->_.flag & CHANNEL_FLAG_CLIENT)
					printf(", connected times: %u\n", channel->_.connected_times);
				else
					putchar('\n');

				if (session) {
					RBTree_t rpc_item_tree;
					RBTreeNode_t* rbnode;
					if (session->f_rpc) {
						rpcFiberCoreCancelAll(session->f_rpc, &rpc_item_tree);
					}
					else if (session->a_rpc) {
						rpcAsyncCoreCancelAll(session->a_rpc, &rpc_item_tree);
					}
					else {
						rbtreeInit(&rpc_item_tree, NULL);
					}
					for (rbnode = rbtreeFirstNode(&rpc_item_tree); rbnode; ) {
						RBTreeNode_t* rbnext = rbtreeNextNode(rbnode);
						rbtreeRemoveNode(&rpc_item_tree, rbnode);
						free(pod_container_of(rbnode, RpcItem_t, m_treenode));
						rbnode = rbnext;
					}
				}
				channelDestroy(channel);
				reactorCommitCmd(channel->_.reactor, &channel->_.freecmd);
			}
			else {
				printf("unknown message type: %d\n", internal->type);
			}
		}
		// handle rpc timeout

		// handle timer event
		for (cur = rbtimerTimeout(&g_Timer, gmtimeMillisecond()); cur; cur = next) {
			RBTimerEvent_t* e = pod_container_of(cur, RBTimerEvent_t, m_listnode);
			next = cur->next;
			if (e->callback(e, e->arg)) {
				rbtimerAddEvent(&g_Timer, e);
			}
			else {
				free(e);
			}
		}
		timer_min_msec = rbtimerMiniumTimestamp(&g_Timer);
		if (timer_min_msec < 0) {
			wait_msec = -1;
		}
		else {
			cur_msec = gmtimeMillisecond();
			if (timer_min_msec > cur_msec)
				wait_msec = timer_min_msec - cur_msec;
			else
				wait_msec = 0;
		}
	}
	// thread exit clean
	if (thread_fiber) {
		fiberFree(thread_fiber);
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
