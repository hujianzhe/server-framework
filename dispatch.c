#include "global.h"
#include "dispatch.h"

typedef struct DispatchItem_t {
	HashtableNode_t m_hashnode;
	DispatchCallback_t func;
} DispatchItem_t;

static Hashtable_t s_NumberDispatchTable;
static HashtableNode_t* s_NumberDispatchBulk[1024];
static int __numkeycmp(const void* node_key, const void* key) { return node_key != key; }
static unsigned int __numkeyhash(const void* key) { return (ptrlen_t)key; }

static Hashtable_t s_StringDispatchTable;
static HashtableNode_t* s_StringDispatchBulk[1024];
static int __strkeycmp(const void* node_key, const void* key) { return strcmp((const char*)node_key, (const char*)key); }
static unsigned int __strkeyhash(const void* key) { return hashBKDR((const char*)key); }

static int default_dispatch_callback(UserMsg_t* m) { return 0; }
DispatchCallback_t g_DefaultDispatchCallback = default_dispatch_callback;

int initDispatch(void) {
	hashtableInit(
		&s_NumberDispatchTable,
		s_NumberDispatchBulk,
		sizeof(s_NumberDispatchBulk) / sizeof(s_NumberDispatchBulk[0]),
		__numkeycmp,
		__numkeyhash
	);

	hashtableInit(
		&s_StringDispatchTable,
		s_StringDispatchBulk,
		sizeof(s_StringDispatchBulk) / sizeof(s_StringDispatchBulk[0]),
		__strkeycmp,
		__strkeyhash
	);
	return 1;
}

int regStringDispatch(const char* str, DispatchCallback_t func) {
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
		exist_node = hashtableInsertNode(&s_StringDispatchTable, &item->m_hashnode);
		if (exist_node != &item->m_hashnode) {
			hashtableReplaceNode(exist_node, &item->m_hashnode);
			free(pod_container_of(exist_node, DispatchItem_t, m_hashnode));
		}
		return 1;
	}
}

int regNumberDispatch(int cmd, DispatchCallback_t func) {
	DispatchItem_t* item = (DispatchItem_t*)malloc(sizeof(DispatchItem_t));
	if (item) {
		HashtableNode_t* exist_node;
		item->m_hashnode.key = (void*)(size_t)cmd;
		item->func = func;
		exist_node = hashtableInsertNode(&s_NumberDispatchTable, &item->m_hashnode);
		if (exist_node != &item->m_hashnode) {
			hashtableReplaceNode(exist_node, &item->m_hashnode);
			free(pod_container_of(exist_node, DispatchItem_t, m_hashnode));
		}
		return 1;
	}
	return 0;
}

DispatchCallback_t getStringDispatch(const char* str) {
	HashtableNode_t* node = hashtableSearchKey(&s_StringDispatchTable, str);
	if (node) {
		return pod_container_of(node, DispatchItem_t, m_hashnode)->func;
	}
	return NULL;
}

DispatchCallback_t getNumberDispatch(int cmd) {
	HashtableNode_t* node = hashtableSearchKey(&s_NumberDispatchTable, (void*)(size_t)cmd);
	if (node) {
		return pod_container_of(node, DispatchItem_t, m_hashnode)->func;
	}
	return NULL;
}

void freeDispatchCallback(void) {
	HashtableNode_t* cur;
	cur = hashtableFirstNode(&s_NumberDispatchTable);
	while (cur) {
		HashtableNode_t* next = hashtableNextNode(cur);
		free(pod_container_of(cur, DispatchItem_t, m_hashnode));
		cur = next;
	}
	hashtableInit(&s_NumberDispatchTable, s_NumberDispatchBulk, sizeof(s_NumberDispatchBulk) / sizeof(s_NumberDispatchBulk[0]), NULL, NULL);

	cur = hashtableFirstNode(&s_StringDispatchTable);
	while (cur) {
		HashtableNode_t* next = hashtableNextNode(cur);
		free((void*)cur->key);
		free(pod_container_of(cur, DispatchItem_t, m_hashnode));
		cur = next;
	}
	hashtableInit(&s_StringDispatchTable, s_StringDispatchBulk, sizeof(s_StringDispatchBulk) / sizeof(s_StringDispatchBulk[0]), NULL, NULL);
}
