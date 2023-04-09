#include "global.h"
#include "dispatch_msg.h"

UserMsg_t* UserMsgExecQueue_next(UserMsgExecQueue_t* dq) {
	ListNode_t* node = listPopNodeFront(&dq->list);
	dq->exec_msg = NULL;
	if (!node) {
		return NULL;
	}
	return pod_container_of(node, UserMsg_t, order_listnode);
}

#ifdef __cplusplus
extern "C" {
#endif

UserMsg_t* newUserMsg(size_t datalen) {
	UserMsg_t* msg = (UserMsg_t*)malloc(sizeof(UserMsg_t) + datalen);
	if (msg) {
		msg->order_dq = NULL;
		msg->channel = NULL;
		msg->peer_addr.sa.sa_family = AF_UNSPEC;
		msg->on_free = NULL;
		msg->hang_up = 0;
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

void freeUserMsg(UserMsg_t* msg) {
	if (!msg) {
		return;
	}
	if (msg->on_free) {
		msg->on_free(msg);
	}
	free(msg);
}

UserMsgExecQueue_t* UserMsgExecQueue_init(UserMsgExecQueue_t* dq) {
	listInit(&dq->list);
	dq->exec_msg = NULL;
	return dq;
}

int UserMsgExecQueue_check_exec(UserMsgExecQueue_t* dq, UserMsg_t* msg) {
	if (dq->exec_msg && dq->exec_msg != msg) {
		if (msg->hang_up) {
			return 0;
		}
		msg->hang_up = 1;
		listPushNodeBack(&dq->list, &msg->order_listnode);
		return 0;
	}
	dq->exec_msg = msg;
	msg->order_dq = dq;
	msg->hang_up = 0;
	return 1;
}

void UserMsgExecQueue_clear(UserMsgExecQueue_t* dq) {
	ListNode_t *cur, *next;

	if (dq->exec_msg) {
		dq->exec_msg->order_dq = NULL;
		dq->exec_msg = NULL;
	}
	for (cur = dq->list.head; cur; cur = next) {
		UserMsg_t* msg = pod_container_of(cur, UserMsg_t, order_listnode);
		next = cur->next;
		if (msg->channel) {
			channelbaseClose(msg->channel);
		}
		freeUserMsg(msg);
	}
	listInit(&dq->list);
}

#ifdef __cplusplus
}
#endif
