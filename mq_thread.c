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

unsigned int THREAD_CALL taskThreadEntry(void* arg) {
	int wait_msec = g_Config.timer_interval_msec;
	long long cur_msec, timer_min_msec;
	long long frame_next_msec = gmtimeMillisecond() + g_Config.timer_interval_msec;
	while (g_Valid) {
		ListNode_t* cur, *next;
		for (cur = dataqueuePop(&g_DataQueue, wait_msec, ~0); cur; cur = next) {
			ReactorCmd_t* internal = (ReactorCmd_t*)cur;
			next = cur->next;
			if (REACTOR_USER_CMD == internal->type) {
				MQRecvMsg_t* ctrl = pod_container_of(internal , MQRecvMsg_t, internal);
				MQDispatchCallback_t callback_func = getDispatchCallback(ctrl->cmd);
				if (!callback_func) {
					continue;
				}
				callback_func(ctrl);
				free(ctrl);
			}
			else if (REACTOR_CHANNEL_FREE_CMD == internal->type) {
				Channel_t* channel = pod_container_of(internal, Channel_t, _.freecmd);
				Session_t* session = (Session_t*)channel->userdata;
				printf("channel(%p) detach, reason:%d", channel, channel->_.detach_error);
				if (channel->_.flag & CHANNEL_FLAG_CLIENT)
					printf(", connected times: %u\n", channel->_.connected_times);
				else
					putchar('\n');
				sessionUnbindChannel(session);
				channelDestroy(channel);
				reactorCommitCmd(channel->_.reactor, &channel->_.freecmd);
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
