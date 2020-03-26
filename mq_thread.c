#include "global.h"
#include "config.h"
#include <stdio.h>

static void msg_handler(RpcFiberCore_t* rpc, ReactorCmd_t* cmdobj) {
	if (REACTOR_USER_CMD == cmdobj->type) {
		MQRecvMsg_t* ctrl = pod_container_of(cmdobj, MQRecvMsg_t, internal);
		Session_t* session = (Session_t*)channelSession(ctrl->channel);
		MQDispatchCallback_t callback = getDispatchCallback(ctrl->cmd);
		if (callback)
			callback(ctrl);
		free(ctrl);
		// test code
		if (session->channel->_.flag & CHANNEL_FLAG_CLIENT) {
			RpcItem_t* rpc_item = rpcFiberCoreExistItem(session->rpc, CMD_RET_TEST);
			if (rpc_item) {
				printf("rpcid(%d) alread send\n", rpc_item->id);
			}
			else {
				char test_data[] = "this text is from client ^.^";
				MQSendMsg_t msg;
				makeMQSendMsg(&msg, CMD_REQ_TEST, test_data, sizeof(test_data));
				channelSendv(session->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
				rpc_item = rpcFiberCoreWaitReturn(session->rpc, CMD_RET_TEST, 1000);
				if (!rpc_item) {
					fputs("rpc call failure", stderr);
				}
				else if (rpc_item->timeout_msec >= 0 &&
					gmtimeMillisecond() - rpc_item->timestamp_msec >= rpc_item->timeout_msec)
				{
					fputs("rpc timeout", stderr);
				}
				else if (rpc_item->ret_msg) {
					MQRecvMsg_t* ret_msg = pod_container_of(rpc_item->ret_msg, MQRecvMsg_t, internal);
					printf("say hello world ... %s\n", ret_msg->data);
					free(ret_msg);
				}
				rpcFiberCoreFreeItem(session->rpc, rpc_item);
			}
		}
	}
	else if (REACTOR_CHANNEL_FREE_CMD == cmdobj->type) {
		Channel_t* channel = pod_container_of(cmdobj, Channel_t, _.freecmd);
		channelDestroy(channel);
		reactorCommitCmd(channel->_.reactor, &channel->_.freecmd);
	}
}

unsigned int THREAD_CALL taskThreadEntry(void* arg) {
	ListNode_t* cur, *next;
	int wait_msec = g_Config.timer_interval_msec;
	long long cur_msec, timer_min_msec;
	long long frame_next_msec = gmtimeMillisecond() + g_Config.timer_interval_msec;
	g_DataFiber = fiberFromThread();
	if (!g_DataFiber) {
		fputs("fiberFromThread error", stderr);
		return 1;
	}
	while (g_Valid) {
		for (cur = dataqueuePop(&g_DataQueue, wait_msec, ~0); cur; cur = next) {
			ReactorCmd_t* internal = (ReactorCmd_t*)cur;
			next = cur->next;
			if (REACTOR_USER_CMD == internal->type) {
				MQRecvMsg_t* ctrl = pod_container_of(internal , MQRecvMsg_t, internal);
				Session_t* session = (Session_t*)channelSession(ctrl->channel);
				if (!session) {
					session = newSession();
					if (!session) {
						channelSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
						free(ctrl);
						continue;
					}
					session->rpc = (RpcFiberCore_t*)malloc(sizeof(RpcFiberCore_t));
					if (!session->rpc) {
						channelSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
						free(ctrl);
						freeSession(session);
						continue;
					}
					if (!rpcFiberCoreInit(session->rpc, g_DataFiber, 0x4000)) {
						channelSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
						free(ctrl);
						free(session->rpc);
						freeSession(session);
						continue;
					}
					session->rpc->msg_handler = (void(*)(RpcFiberCore_t*, void*))msg_handler;
					sessionBindChannel(session, ctrl->channel);
				}

				if (ctrl->cmd < CMD_RPC_RET_START) {
					rpcFiberCoreMessageHandleSwitch(session->rpc, internal);
				}
				else if (!rpcFiberCoreReturnSwitch(session->rpc, ctrl->cmd, internal)) {
					free(ctrl);
					continue;
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

				if (session && session->rpc) {
					// TODO delay free session
					rpcFiberCoreDisconnectHandleSwitch(session->rpc, internal);
				}
				else {
					channelDestroy(channel);
					reactorCommitCmd(channel->_.reactor, &channel->_.freecmd);
				}
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
		cur_msec = gmtimeMillisecond();

		if (timer_min_msec < 0 || timer_min_msec >= frame_next_msec) {
			if (cur_msec <= frame_next_msec)
				wait_msec = frame_next_msec - cur_msec;
			else {
				frame_next_msec += g_Config.timer_interval_msec;
			}
		}
		else if (cur_msec <= timer_min_msec)
			wait_msec = timer_min_msec - cur_msec;
		else
			wait_msec = 0;
	}
	// thread exit clean
	fiberFree(g_DataFiber);
	for (cur = rbtimerClean(&g_Timer); cur; cur = next) {
		free(pod_container_of(cur, RBTimerEvent_t, m_listnode));
		next = cur->next;
	}
	for (cur = dataqueueClean(&g_DataQueue); cur; cur = next) {
		ReactorCmd_t* internal = (ReactorCmd_t*)cur;
		next = cur->next;
		if (REACTOR_USER_CMD == internal->type)
			free(pod_container_of(internal, MQRecvMsg_t, internal));
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
