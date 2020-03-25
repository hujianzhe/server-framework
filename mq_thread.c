#include "global.h"
#include "config.h"
#include <stdio.h>

static MQRecvMsg_t* sessionRpcSwitchTo(Session_t* session) {
	MQRecvMsg_t* ret_msg;
	fiberSwitch(session->fiber, session->sche_fiber);
	while (session->fiber_new_msg) {
		MQDispatchCallback_t callback;
		MQRecvMsg_t* ctrl = session->fiber_new_msg;
		session->fiber_new_msg = NULL;
		callback = getDispatchCallback(ctrl->cmd);
		if (callback) {
			callback(ctrl);
		}
		free(ctrl);
		fiberSwitch(session->fiber, session->sche_fiber);
	}
	ret_msg = session->fiber_ret_msg;
	session->fiber_ret_msg = NULL;
	return ret_msg;
}

static void sessionFiberProc(Fiber_t* fiber) {
	Session_t* session = (Session_t*)fiber->arg;
	while (1) {
		session->fiber_busy = 1;
		if (session->fiber_net_disconnect_cmd) {
			Channel_t* channel = pod_container_of(session->fiber_net_disconnect_cmd, Channel_t, _.freecmd);
			channelDestroy(channel);
			reactorCommitCmd(channel->_.reactor, &channel->_.freecmd);
			session->fiber_net_disconnect_cmd = NULL;
		}
		else if (session->fiber_new_msg) {
			MQRecvMsg_t* ctrl = session->fiber_new_msg;
			session->fiber_new_msg = NULL;
			MQDispatchCallback_t callback = getDispatchCallback(ctrl->cmd);
			if (callback) {
				callback(ctrl);
			}
			free(ctrl);
			// test code
			if (session->channel->_.flag & CHANNEL_FLAG_CLIENT) {
				MQRecvMsg_t* ret_msg;
				char test_data[] = "this text is from client ^.^";
				MQSendMsg_t msg;
				makeMQSendMsg(&msg, CMD_REQ_TEST, test_data, sizeof(test_data));
				channelSendv(session->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
				regSessionRpc(session, CMD_RET_TEST);
				session->fiber_wait_timestamp_msec = gmtimeMillisecond();
				session->fiber_wait_timeout_msec = 1000;
				ret_msg = sessionRpcSwitchTo(session);
				if (session->fiber_wait_timeout_msec >= 0 &&
					gmtimeMillisecond() - session->fiber_wait_timestamp_msec >= session->fiber_wait_timeout_msec)
				{
					fputs("rpc timeout", stderr);
				}
				else {
					printf("say hello world ... %s\n", ret_msg->data);
				}
				free(ret_msg);
			}
		}
		session->fiber_busy = 0;
		fiberSwitch(fiber, session->sche_fiber);
	}
	fiberSwitch(fiber, session->sche_fiber);
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
					session->fiber = fiberCreate(g_DataFiber, 0x4000, sessionFiberProc);
					if (!session->fiber) {
						channelSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
						free(ctrl);
						freeSession(session);
						continue;
					}
					session->fiber->arg = session;
					session->sche_fiber = g_DataFiber;
					sessionBindChannel(session, ctrl->channel);
				}

				if (ctrl->cmd < CMD_RPC_RET_START) {
					session->fiber_new_msg = ctrl;
					fiberSwitch(g_DataFiber, session->fiber);
				}
				else if (session->fiber_busy) {
					if (existAndDeleteSessionRpc(session, ctrl->cmd)) {
						session->fiber_ret_msg = ctrl;
						fiberSwitch(g_DataFiber, session->fiber);
					}
					else {
						free(ctrl);
					}
				}
				else {
					free(ctrl);
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
					// TODO delay free session
					sessionUnbindChannel(session);
					session->fiber_net_disconnect_cmd = internal;
					if (!session->fiber_busy) {
						fiberSwitch(g_DataFiber, session->fiber);
					}
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
