#include "global.h"
#include "config.h"
#include <stdio.h>

static void freeMQSocketMsg(ListNode_t* node) {
	ReactorCmd_t* internal = (ReactorCmd_t*)node;
	if (REACTOR_USER_CMD == internal->type)
		free(pod_container_of(internal, MQRecvMsg_t, internal));
}

static void freeTimerEvent(RBTimerEvent_t* e) {
	free(e);
}

static void sessionFiberProc(Fiber_t* fiber) {
	Session_t* session = (Session_t*)fiber->arg;
	while (1) {
		ListNode_t* cur, *next;
		for (cur = session->fiber_cmdlist.head; cur; cur = next) {
			ReactorCmd_t* internal = (ReactorCmd_t*)cur;
			next = cur->next;
			listRemoveNode(&session->fiber_cmdlist, cur);
			session->fiber_busy = 1;

			if (REACTOR_CHANNEL_FREE_CMD == internal->type) {
				Channel_t* channel = pod_container_of(internal, Channel_t, _.freecmd);
				channelDestroy(channel);
				reactorCommitCmd(channel->_.reactor, &channel->_.freecmd);
				break;
			}
			else {
				MQRecvMsg_t* ctrl = pod_container_of(internal, MQRecvMsg_t, internal);
				MQDispatchCallback_t callback = getDispatchCallback(ctrl->cmd);
				if (callback) {
					callback(ctrl);
				}
				free(ctrl);

				if (session->channel->_.flag & CHANNEL_FLAG_CLIENT) {
					char test_data[] = "this text is from client ^.^";
					MQSendMsg_t msg;
					makeMQSendMsg(&msg, CMD_REQ_TEST, test_data, sizeof(test_data));
					channelSendv(session->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
					regSessionRpc(session, CMD_RET_TEST);
					session->fiber_wait_timestamp_msec = gmtimeMillisecond();
					session->fiber_wait_timeout_msec = 1000;
					fiberSwitch(fiber, g_DataFiber);
					if (gmtimeMillisecond() - session->fiber_wait_timestamp_msec >= session->fiber_wait_timeout_msec) {
						fputs("rpc timeout", stderr);
					}
					else {
						printf("say hello world ... %s\n", session->fiber_return_data);
						freeSessionReturnData(session);
					}
				}
			}
		}
		session->fiber_busy = 0;
		fiberSwitch(fiber, g_DataFiber);
	}
	fiberSwitch(fiber, g_DataFiber);
}

unsigned int THREAD_CALL taskThreadEntry(void* arg) {
	int wait_msec = g_Config.timer_interval_msec;
	long long cur_msec, timer_min_msec;
	long long frame_next_msec = gmtimeMillisecond() + g_Config.timer_interval_msec;
	g_DataFiber = fiberFromThread();
	if (!g_DataFiber) {
		fputs("fiberFromThread error", stderr);
		return 1;
	}
	while (g_Valid) {
		ListNode_t* cur, *next;
		for (cur = dataqueuePop(&g_DataQueue, wait_msec, ~0); cur; cur = next) {
			ReactorCmd_t* internal = (ReactorCmd_t*)cur;
			next = cur->next;
			if (REACTOR_USER_CMD == internal->type) {
				MQRecvMsg_t* ctrl = pod_container_of(internal , MQRecvMsg_t, internal);
				Session_t* session = channelSession(ctrl->channel);
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
					sessionBindChannel(session, ctrl->channel);
				}

				if (ctrl->cmd < CMD_RPC_RET_START) {
					listPushNodeBack(&session->fiber_cmdlist, cur);
					if (!session->fiber_busy) {
						fiberSwitch(g_DataFiber, session->fiber);
					}
				}
				else if (session->fiber_busy) {
					if (existAndDeleteSessionRpc(session, ctrl->cmd)) {
						if (!saveSessionReturnData(session, ctrl->data, ctrl->datalen)) {
							free(ctrl);
							continue;
						}
						free(ctrl);
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
				Session_t* session = channelSession(channel);
				printf("channel(%p) detach, reason:%d", channel, channel->_.detach_error);
				if (channel->_.flag & CHANNEL_FLAG_CLIENT)
					printf(", connected times: %u\n", channel->_.connected_times);
				else
					putchar('\n');
				if (session) {
					// TODO delay free session
					sessionUnbindChannel(session);
					listPushNodeBack(&session->fiber_cmdlist, cur);
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
		rbtimerCall(&g_Timer, gmtimeMillisecond(), freeTimerEvent);

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
	rbtimerClean(&g_Timer, freeTimerEvent);
	dataqueueClean(&g_DataQueue, freeMQSocketMsg);
	fiberFree(g_DataFiber);
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
