#include "global.h"
#include "mq_dispatch.h"

typedef struct DispatchItem_t {
	HashtableNode_t m_hashnode;
	union {
		MQDispatchCallback_t func;
		Fiber_t* fiber;
	};
} DispatchItem_t;

Hashtable_t g_DispatchTable;
static HashtableNode_t* s_DispatchBulk[1024];
Hashtable_t g_DispatchRpcCtxTable;
static HashtableNode_t* s_DispatchRpcCtxBulk[1024];
static int __keycmp(const void* node_key, const void* key) { return node_key != key; }
static unsigned int __keyhash(const void* key) { return (ptrlen_t)key; }

int initDispatch(void) {
	hashtableInit(
		&g_DispatchTable,
		s_DispatchBulk,
		sizeof(s_DispatchBulk) / sizeof(s_DispatchBulk[0]),
		__keycmp,
		__keyhash
	);
	hashtableInit(
		&g_DispatchRpcCtxTable,
		s_DispatchRpcCtxBulk,
		sizeof(s_DispatchRpcCtxBulk) / sizeof(s_DispatchRpcCtxBulk[0]),
		__keycmp,
		__keyhash
	);
	return 1;
}

int regDispatchCallback(int cmd, MQDispatchCallback_t func) {
	DispatchItem_t* item = (DispatchItem_t*)malloc(sizeof(DispatchItem_t));
	if (item) {
		HashtableNode_t* exist_node;
		item->m_hashnode.key = (void*)(size_t)cmd;
		item->func = func;
		exist_node = hashtableInsertNode(&g_DispatchTable, &item->m_hashnode);
		if (exist_node != &item->m_hashnode) {
			hashtableReplaceNode(exist_node, &item->m_hashnode);
			free(pod_container_of(exist_node, DispatchItem_t, m_hashnode));
		}
		return 1;
	}
	return 0;
}

MQDispatchCallback_t getDispatchCallback(int cmd) {
	HashtableNode_t* node = hashtableSearchKey(&g_DispatchTable, (void*)(size_t)cmd);
	if (node) {
		return pod_container_of(node, DispatchItem_t, m_hashnode)->func;
	}
	return NULL;
}

void freeDispatchCallback(void) {
	HashtableNode_t* cur = hashtableFirstNode(&g_DispatchTable);
	while (cur) {
		HashtableNode_t* next = hashtableNextNode(cur);
		free(pod_container_of(cur, DispatchItem_t, m_hashnode));
		cur = next;
	}
	hashtableInit(&g_DispatchTable, s_DispatchBulk, sizeof(s_DispatchBulk) / sizeof(s_DispatchBulk[0]), NULL, NULL);
}

int regDispatchRpcContext(int cmd, Fiber_t* fiber) {
	DispatchItem_t* item = (DispatchItem_t*)malloc(sizeof(DispatchItem_t));
	if (item) {
		HashtableNode_t* exist_node;
		item->m_hashnode.key = (void*)(size_t)cmd;
		item->fiber = fiber;
		exist_node = hashtableInsertNode(&g_DispatchRpcCtxTable, &item->m_hashnode);
		if (exist_node == &item->m_hashnode) {
			return 1;
		}
		free(item);
	}
	return 0;
}

Fiber_t* getDispatchRpcContext(int cmd) {
	HashtableNode_t* node = hashtableSearchKey(&g_DispatchRpcCtxTable, (void*)(size_t)cmd);
	if (node) {
		return pod_container_of(node, DispatchItem_t, m_hashnode)->fiber;
	}
	return NULL;
}