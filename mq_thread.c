#include "global.h"
#include "config.h"
#include <stdio.h>

static void msg_handler(RpcFiberCore_t* rpc, UserMsg_t* ctrl) {
	Session_t* session = (Session_t*)channelSession(ctrl->channel);
	DispatchCallback_t callback = getDispatchCallback(ctrl->cmd);
	if (callback)
		callback(ctrl);
	free(ctrl);
	// test code
	if (session->channel->_.flag & CHANNEL_FLAG_CLIENT) {
		static int times;
		while (10 > times) {
			RpcItem_t* rpc_item = (RpcItem_t*)malloc(sizeof(RpcItem_t));
			if (!rpc_item) {
				break;
			}
			rpcItemSet(rpc_item, rpcGenId(), 1000);
			if (!rpcFiberCoreRegItem(rpc, rpc_item)) {
				printf("rpcid(%d) already send\n", rpc_item->id);
				free(rpc_item);
				times++;
				break;
			}
			else {
				long long now_msec, cost_msec;
				char test_data[] = "this text is from client ^.^";
				SendMsg_t msg;
				makeSendMsgRpcReq(&msg, CMD_REQ_TEST, rpc_item->id, test_data, sizeof(test_data));
				channelShardSendv(session->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
				rpc_item->timestamp_msec = gmtimeMillisecond();
				printf("rpc(%d) start send, send_msec=%lld\n", CMD_RET_TEST, rpc_item->timestamp_msec);
				rpc_item = rpcFiberCoreYield(rpc);

				now_msec = gmtimeMillisecond();
				cost_msec = now_msec - rpc_item->timestamp_msec;
				if (rpc_item->timeout_msec >= 0 && cost_msec >= rpc_item->timeout_msec) {
					puts("rpc call timeout");
				}
				else if (rpc_item->ret_msg) {
					UserMsg_t* ret_msg = (UserMsg_t*)rpc_item->ret_msg;
					printf("time cost(%lld msec) say hello world ... %s\n", cost_msec, ret_msg->data);
				}
			}
			times++;
		}
	}
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
						DispatchCallback_t callback = getDispatchCallback(ctrl->cmd);
						if (callback)
							callback(ctrl);
						free(ctrl);

						// test code
						if (session->channel->_.flag & CHANNEL_FLAG_CLIENT) {
							static int times;
							if (10 > times) {
								RpcItem_t* rpc_item = (RpcItem_t*)malloc(sizeof(RpcItem_t));
								if (!rpc_item) {
									continue;
								}
								rpcItemSet(rpc_item, rpcGenId(), 1000);
								if (!rpcAsyncCoreRegItem(session->a_rpc, rpc_item, NULL, rpcRetTest)) {
									printf("rpcid(%d) already send\n", rpc_item->id);
									free(rpc_item);
								}
								else {
									char test_data[] = "this text is from client ^.^";
									SendMsg_t msg;
									makeSendMsgRpcReq(&msg, CMD_REQ_TEST, rpc_item->id, test_data, sizeof(test_data));
									channelShardSendv(session->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
									rpc_item->timestamp_msec = gmtimeMillisecond();
									printf("rpc(%d) start send, send_msec=%lld\n", CMD_RET_TEST, rpc_item->timestamp_msec);
								}
								times++;
							}
						}
					}
				}
				else {
					DispatchCallback_t callback = getDispatchCallback(ctrl->cmd);
					if (callback)
						callback(ctrl);
					free(ctrl);

					// test code
					if (session->channel->_.flag & CHANNEL_FLAG_CLIENT) {
						static int times;
						if (10 > times) {
							char test_data[] = "this text is from client ^.^";
							SendMsg_t msg;
							makeSendMsg(&msg, CMD_REQ_TEST, test_data, sizeof(test_data));
							channelShardSendv(session->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
							times++;
						}
					}
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

				if (session && session->f_rpc) {
					// rpcFiberCoreMessageHandleSwitch(session->f_rpc, NULL);
					// TODO delay free session
				}
				channelDestroy(channel);
				reactorCommitCmd(channel->_.reactor, &channel->_.freecmd);
			}
			else {
				printf("unknown message type: %d\n", internal->type);
			}
		}
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
