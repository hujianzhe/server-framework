#include "global.h"
#include "dispatch_msg.h"

static void free_user_msg(UserMsg_t* msg) {
	free(msg);
}

#ifdef __cplusplus
extern "C" {
#endif

UserMsg_t* newUserMsg(size_t datalen) {
	UserMsg_t* msg = (UserMsg_t*)malloc(sizeof(UserMsg_t) + datalen);
	if (msg) {
		msg->channel = NULL;
		msg->peer_addr.sa.sa_family = AF_UNSPEC;
		msg->on_free = free_user_msg;
		msg->retry = 0;
		msg->param.type = 0;
		msg->param.value = NULL;
		msg->enqueue_time_msec = -1;
		msg->callback = NULL;
		msg->retcode = 0;
		msg->rpcid = 0;
		msg->datalen = datalen;
		msg->data[msg->datalen] = 0;
	}
	return msg;
}

UserMsgExecQueue_t* UserMsgExecQueue_init(UserMsgExecQueue_t* dq) {
	listInit(&dq->list);
	dq->waiting = 0;
	dq->removed = 0;
	return dq;
}

int UserMsgExecQueue_try_exec(UserMsgExecQueue_t* dq, TaskThread_t* thrd, UserMsg_t* msg) {
	ListNode_t* node;

	if (!msg->retry) {
		if (msg->channel) {
			channelbaseAddRef(msg->channel);
		}
		if (dq->waiting) {
			listPushNodeBack(&dq->list, &msg->order_listnode);
			return 0;
		}
		dq->waiting = 1;
	}
	msg->callback(thrd, msg);
	if (msg->channel) {
		channelbaseClose(msg->channel);
	}
	node = listPopNodeFront(&dq->list);
	if (!node) {
		dq->waiting = 0;
		return 1;
	}
	msg = pod_container_of(node, UserMsg_t, order_listnode);
	msg->retry = 1;
	StackCoSche_function(thrd->sche, TaskThread_call_dispatch, msg, (void(*)(void*))msg->on_free);
	return 1;
}

void UserMsgExecQueue_clear(UserMsgExecQueue_t* dq) {
	ListNode_t *cur, *next;
	for (cur = dq->list.head; cur; cur = next) {
		UserMsg_t* msg = pod_container_of(cur, UserMsg_t, order_listnode);
		next = cur->next;
		msg->on_free(msg);
	}
	listInit(&dq->list);
	dq->waiting = 0;
}

#ifdef __cplusplus
}
#endif
