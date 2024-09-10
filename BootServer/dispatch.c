#include "global.h"
#include "dispatch.h"

typedef struct DispatchItem_t {
	HashtableNode_t m_hashnode;
	DispatchNetCallback_t func;
} DispatchItem_t;

typedef struct Dispatch_t {
	Hashtable_t number_table;
	HashtableNode_t* number_table_bulks[1024];
	Hashtable_t string_table;
	HashtableNode_t* string_table_bulks[1024];
} Dispatch_t;

Dispatch_t* newDispatch(void) {
	Dispatch_t* dispatch = (Dispatch_t*)malloc(sizeof(Dispatch_t));
	if (dispatch) {
		hashtableInit(
			&dispatch->number_table,
			dispatch->number_table_bulks,
			sizeof(dispatch->number_table_bulks) / sizeof(dispatch->number_table_bulks[0]),
			hashtableDefaultKeyCmp32,
			hashtableDefaultKeyHash32
		);

		hashtableInit(
			&dispatch->string_table,
			dispatch->string_table_bulks,
			sizeof(dispatch->string_table_bulks) / sizeof(dispatch->string_table_bulks[0]),
			hashtableDefaultKeyCmpStr,
			hashtableDefaultKeyHashStr
		);
	}
	return dispatch;
}

void freeDispatch(Dispatch_t* dispatch) {
	if (dispatch) {
		HashtableNode_t* cur;
		cur = hashtableFirstNode(&dispatch->number_table);
		while (cur) {
			HashtableNode_t* next = hashtableNextNode(cur);
			free(pod_container_of(cur, DispatchItem_t, m_hashnode));
			cur = next;
		}
		cur = hashtableFirstNode(&dispatch->string_table);
		while (cur) {
			HashtableNode_t* next = hashtableNextNode(cur);
			free((void*)(cur->key.ptr));
			free(pod_container_of(cur, DispatchItem_t, m_hashnode));
			cur = next;
		}
		free(dispatch);
	}
}

/**************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

int regStringDispatch(Dispatch_t* dispatch, const char* str, DispatchNetCallback_t func) {
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
		exist_node = hashtableInsertNode(&dispatch->string_table, &item->m_hashnode);
		if (exist_node != &item->m_hashnode) {
			DispatchItem_t* exist_item = pod_container_of(exist_node, DispatchItem_t, m_hashnode);
			hashtableReplaceNode(&dispatch->string_table, exist_node, &item->m_hashnode);
			free((void*)(exist_node->key.ptr));
			free(exist_item);
		}
		return 1;
	}
}

int regNumberDispatch(Dispatch_t* dispatch, int cmd, DispatchNetCallback_t func) {
	DispatchItem_t* item = (DispatchItem_t*)malloc(sizeof(DispatchItem_t));
	if (item) {
		HashtableNode_t* exist_node;
		item->m_hashnode.key.i32 = cmd;
		item->func = func;
		exist_node = hashtableInsertNode(&dispatch->number_table, &item->m_hashnode);
		if (exist_node != &item->m_hashnode) {
			hashtableReplaceNode(&dispatch->number_table, exist_node, &item->m_hashnode);
			free(pod_container_of(exist_node, DispatchItem_t, m_hashnode));
		}
		return 1;
	}
	return 0;
}

DispatchNetCallback_t getStringDispatch(const Dispatch_t* dispatch, const char* str) {
	HashtableNodeKey_t hkey;
	HashtableNode_t* node;
	hkey.ptr = str;
	node = hashtableSearchKey(&dispatch->string_table, hkey);
	if (node) {
		return pod_container_of(node, DispatchItem_t, m_hashnode)->func;
	}
	return NULL;
}

DispatchNetCallback_t getNumberDispatch(const Dispatch_t* dispatch, int cmd) {
	HashtableNodeKey_t hkey;
	HashtableNode_t* node;
	hkey.i32 = cmd;
	node = hashtableSearchKey(&dispatch->number_table, hkey);
	if (node) {
		return pod_container_of(node, DispatchItem_t, m_hashnode)->func;
	}
	return NULL;
}

#ifdef __cplusplus
}
#endif
