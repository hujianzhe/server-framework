#include "global.h"
#include "dispatch.h"

typedef struct DispatchItem_t {
	HashtableNode_t m_hashnode;
	DispatchCallback_t func;
} DispatchItem_t;

static void free_user_msg(UserMsg_t* msg) {
	if (USER_MSG_EXTRA_HTTP_FRAME == msg->param.type) {
		free(httpframeReset(msg->param.httpframe));
	}
	free(msg);
}

#ifdef __cplusplus
extern "C" {
#endif

UserMsg_t* newUserMsg(size_t datalen) {
	UserMsg_t* msg = (UserMsg_t*)malloc(sizeof(UserMsg_t) + datalen);
	if (msg) {
		msg->internal.type = REACTOR_USER_CMD;
		msg->channel = NULL;
		msg->peer_addr.sa.sa_family = AF_UNSPEC;
		msg->on_free = free_user_msg;
		msg->be_from_cluster = 0;
		msg->param.type = 0;
		msg->param.httpframe = NULL;
		msg->param.timer_event = NULL;
		msg->enqueue_time_msec = -1;
		msg->cmdstr = NULL;
		msg->rpc_status = 0;
		msg->cmdid = 0;
		msg->retcode = 0;
		msg->rpcid = 0;
		msg->datalen = datalen;
		msg->data[msg->datalen] = 0;
	}
	return msg;
}

Dispatch_t* newDispatch(void) {
	Dispatch_t* dispatch = (Dispatch_t*)malloc(sizeof(Dispatch_t));
	if (dispatch) {
		dispatch->null_dispatch_callback = NULL;

		hashtableInit(
			&dispatch->s_NumberDispatchTable,
			dispatch->s_NumberDispatchBulk,
			sizeof(dispatch->s_NumberDispatchBulk) / sizeof(dispatch->s_NumberDispatchBulk[0]),
			hashtableDefaultKeyCmp32,
			hashtableDefaultKeyHash32
		);

		hashtableInit(
			&dispatch->s_StringDispatchTable,
			dispatch->s_StringDispatchBulk,
			sizeof(dispatch->s_StringDispatchBulk) / sizeof(dispatch->s_StringDispatchBulk[0]),
			hashtableDefaultKeyCmpStr,
			hashtableDefaultKeyHashStr
		);
	}
	return dispatch;
}

int regStringDispatch(Dispatch_t* dispatch, const char* str, DispatchCallback_t func) {
	DispatchItem_t* item = (DispatchItem_t*)malloc(sizeof(DispatchItem_t));
	if (!item) {
		return 0;
	}
	str = strdup(str);
	if (!str) {
		free(item);
		return 0;
	}
	else {
		HashtableNode_t* exist_node;
		item->m_hashnode.key.ptr = str;
		item->func = func;
		exist_node = hashtableInsertNode(&dispatch->s_StringDispatchTable, &item->m_hashnode);
		if (exist_node != &item->m_hashnode) {
			hashtableReplaceNode(&dispatch->s_StringDispatchTable, exist_node, &item->m_hashnode);
			free(pod_container_of(exist_node, DispatchItem_t, m_hashnode));
		}
		return 1;
	}
}

int regNumberDispatch(Dispatch_t* dispatch, int cmd, DispatchCallback_t func) {
	DispatchItem_t* item = (DispatchItem_t*)malloc(sizeof(DispatchItem_t));
	if (item) {
		HashtableNode_t* exist_node;
		item->m_hashnode.key.i32 = cmd;
		item->func = func;
		exist_node = hashtableInsertNode(&dispatch->s_NumberDispatchTable, &item->m_hashnode);
		if (exist_node != &item->m_hashnode) {
			hashtableReplaceNode(&dispatch->s_NumberDispatchTable, exist_node, &item->m_hashnode);
			free(pod_container_of(exist_node, DispatchItem_t, m_hashnode));
		}
		return 1;
	}
	return 0;
}

DispatchCallback_t getStringDispatch(Dispatch_t* dispatch, const char* str) {
	HashtableNodeKey_t hkey;
	HashtableNode_t* node;
	hkey.ptr = str;
	node = hashtableSearchKey(&dispatch->s_StringDispatchTable, hkey);
	if (node) {
		return pod_container_of(node, DispatchItem_t, m_hashnode)->func;
	}
	return NULL;
}

DispatchCallback_t getNumberDispatch(Dispatch_t* dispatch, int cmd) {
	HashtableNodeKey_t hkey;
	HashtableNode_t* node;
	hkey.i32 = cmd;
	node = hashtableSearchKey(&dispatch->s_NumberDispatchTable, hkey);
	if (node) {
		return pod_container_of(node, DispatchItem_t, m_hashnode)->func;
	}
	return NULL;
}

void freeDispatch(Dispatch_t* dispatch) {
	if (dispatch) {
		HashtableNode_t* cur;
		cur = hashtableFirstNode(&dispatch->s_NumberDispatchTable);
		while (cur) {
			HashtableNode_t* next = hashtableNextNode(cur);
			free(pod_container_of(cur, DispatchItem_t, m_hashnode));
			cur = next;
		}
		cur = hashtableFirstNode(&dispatch->s_StringDispatchTable);
		while (cur) {
			HashtableNode_t* next = hashtableNextNode(cur);
			free((void*)(cur->key.ptr));
			free(pod_container_of(cur, DispatchItem_t, m_hashnode));
			cur = next;
		}
		free(dispatch);
	}
}

#ifdef __cplusplus
}
#endif
