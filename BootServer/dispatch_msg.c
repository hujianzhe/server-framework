#include "global.h"
#include "dispatch_msg.h"

SerialExecObj_t* SerialExecQueue_next(SerialExecQueue_t* dq) {
	ListNode_t* node = listPopNodeFront(&dq->list);
	dq->exec_obj = NULL;
	if (!node) {
		return NULL;
	}
	return pod_container_of(node, SerialExecObj_t, listnode);
}

#ifdef __cplusplus
extern "C" {
#endif

UserMsg_t* newUserMsg(size_t datalen) {
	UserMsg_t* msg = (UserMsg_t*)malloc(sizeof(UserMsg_t) + datalen);
	if (msg) {
		memset(&msg->serial, 0, sizeof(msg->serial));
		msg->channel = NULL;
		msg->peer_addr.sa.sa_family = AF_UNSPEC;
		msg->on_free = NULL;
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

void freeUserMsgSerial(SerialExecObj_t* serial) {
	UserMsg_t* msg = pod_container_of(serial, UserMsg_t, serial);
	if (msg->channel) {
		channelbaseClose(msg->channel);
	}
	freeUserMsg(msg);
}

SerialExecQueue_t* SerialExecQueue_init(SerialExecQueue_t* dq) {
	listInit(&dq->list);
	dq->exec_obj = NULL;
	return dq;
}

int SerialExecQueue_check_exec(SerialExecQueue_t* dq, SerialExecObj_t* obj) {
	if (dq->exec_obj && dq->exec_obj != obj) {
		if (obj->hang_up) {
			return 0;
		}
		obj->hang_up = 1;
		listPushNodeBack(&dq->list, &obj->listnode);
		return 0;
	}
	dq->exec_obj = obj;
	obj->dq = dq;
	obj->hang_up = 0;
	return 1;
}

void SerialExecQueue_clear(SerialExecQueue_t* dq, void(*fn_free)(SerialExecObj_t*)) {
	if (dq->exec_obj) {
		dq->exec_obj->dq = NULL;
		dq->exec_obj = NULL;
	}
	if (fn_free) {
		ListNode_t *cur, *next;
		for (cur = dq->list.head; cur; cur = next) {
			SerialExecObj_t* obj = pod_container_of(cur, SerialExecObj_t, listnode);
			next = cur->next;

			fn_free(obj);
		}
	}
	listInit(&dq->list);
}

#ifdef __cplusplus
}
#endif
