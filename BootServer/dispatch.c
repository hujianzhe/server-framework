#include "global.h"
#include "dispatch.h"

typedef struct DispatchItem_t {
	HashtableNode_t m_hashnode;
	DispatchCallback_t func;
} DispatchItem_t;

static int __numkeycmp(const void* node_key, const void* key) { return node_key != key; }
static unsigned int __numkeyhash(const void* key) { return (ptrlen_t)key; }

static int __strkeycmp(const void* node_key, const void* key) { return strcmp((const char*)node_key, (const char*)key); }
static unsigned int __strkeyhash(const void* key) { return hashBKDR((const char*)key); }

DispatchCallback_t g_DefaultDispatchCallback = NULL;

#ifdef __cplusplus
extern "C" {
#endif

UserMsg_t* newUserMsg(size_t datalen) {
	UserMsg_t* msg = (UserMsg_t*)malloc(sizeof(UserMsg_t) + datalen);
	if (msg) {
		msg->internal.type = REACTOR_USER_CMD;
		msg->channel = NULL;
		msg->peer_addr.sa.sa_family = AF_UNSPEC;
		msg->be_from_cluster = 0;
		msg->extra_type = 0;
		msg->httpframe = NULL;
		msg->timer_event = NULL;
		msg->cmdstr = NULL;
		msg->datalen = datalen;
		msg->data[msg->datalen] = 0;
	}
	return msg;
}

void set_g_DefaultDispatchCallback(DispatchCallback_t fn) { g_DefaultDispatchCallback = fn; }

Dispatch_t* newDispatch(void) {
	Dispatch_t* dispatch = (Dispatch_t*)malloc(sizeof(Dispatch_t));
	if (dispatch) {
		hashtableInit(
			&dispatch->s_NumberDispatchTable,
			dispatch->s_NumberDispatchBulk,
			sizeof(dispatch->s_NumberDispatchBulk) / sizeof(dispatch->s_NumberDispatchBulk[0]),
			__numkeycmp,
			__numkeyhash
		);

		hashtableInit(
			&dispatch->s_StringDispatchTable,
			dispatch->s_StringDispatchBulk,
			sizeof(dispatch->s_StringDispatchBulk) / sizeof(dispatch->s_StringDispatchBulk[0]),
			__strkeycmp,
			__strkeyhash
		);
	}
	return dispatch;
}

int regStringDispatch(Dispatch_t* dispatch, const char* str, DispatchCallback_t func) {
	DispatchItem_t* item = (DispatchItem_t*)malloc(sizeof(DispatchItem_t));
	if (!item)
		return 0;
	str = strdup(str);
	if (!str) {
		free(item);
		return 0;
	}
	else {
		HashtableNode_t* exist_node;
		item->m_hashnode.key = str;
		item->func = func;
		exist_node = hashtableInsertNode(&dispatch->s_StringDispatchTable, &item->m_hashnode);
		if (exist_node != &item->m_hashnode) {
			hashtableReplaceNode(exist_node, &item->m_hashnode);
			free(pod_container_of(exist_node, DispatchItem_t, m_hashnode));
		}
		return 1;
	}
}

int regNumberDispatch(Dispatch_t* dispatch, int cmd, DispatchCallback_t func) {
	DispatchItem_t* item = (DispatchItem_t*)malloc(sizeof(DispatchItem_t));
	if (item) {
		HashtableNode_t* exist_node;
		item->m_hashnode.key = (void*)(size_t)cmd;
		item->func = func;
		exist_node = hashtableInsertNode(&dispatch->s_NumberDispatchTable, &item->m_hashnode);
		if (exist_node != &item->m_hashnode) {
			hashtableReplaceNode(exist_node, &item->m_hashnode);
			free(pod_container_of(exist_node, DispatchItem_t, m_hashnode));
		}
		return 1;
	}
	return 0;
}

DispatchCallback_t getStringDispatch(Dispatch_t* dispatch, const char* str) {
	HashtableNode_t* node = hashtableSearchKey(&dispatch->s_StringDispatchTable, str);
	if (node) {
		return pod_container_of(node, DispatchItem_t, m_hashnode)->func;
	}
	return NULL;
}

DispatchCallback_t getNumberDispatch(Dispatch_t* dispatch, int cmd) {
	HashtableNode_t* node = hashtableSearchKey(&dispatch->s_NumberDispatchTable, (void*)(size_t)cmd);
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
			free((void*)cur->key);
			free(pod_container_of(cur, DispatchItem_t, m_hashnode));
			cur = next;
		}
		free(dispatch);
	}
}

#ifdef __cplusplus
}
#endif
