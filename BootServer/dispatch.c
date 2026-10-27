#include "global.h"
#include "dispatch.h"
#include <string.h>

typedef struct DispatchItem_t {
	HashtableNode_t m_hashnode;
	DispatchNetCallback_t func;
} DispatchItem_t;

typedef struct DispatchItemBlock_t {
	ListNode_t m_listnode;
	DispatchItem_t m_items[146];
} DispatchItemBlock_t;

typedef struct Dispatch_t {
	Hashtable_t number_table;
	HashtableNode_t* number_table_bulks[1024];
	Hashtable_t string_table;
	HashtableNode_t* string_table_bulks[1024];
	size_t items_pop_idx;
	List_t item_block_list;
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

		dispatch->items_pop_idx = 0;
		listInit(&dispatch->item_block_list);
	}
	return dispatch;
}

void freeDispatch(Dispatch_t* dispatch) {
	if (dispatch) {
		ListNode_t* lcur;
		HashtableNode_t* hcur;

		hcur = hashtableFirstNode(&dispatch->string_table);
		while (hcur) {
			HashtableNode_t* next = hashtableNextNode(hcur);
			free(pod_container_of(hcur, DispatchItem_t, m_hashnode));
			hcur = next;
		}

		lcur = dispatch->item_block_list.head;
		while (lcur) {
			ListNode_t* next = lcur->next;
			free(pod_container_of(lcur, DispatchItemBlock_t, m_listnode));
			lcur = next;
		}
		free(dispatch);
	}
}

static DispatchItem_t* pop_new_item(Dispatch_t* dispatch) {
	DispatchItemBlock_t* block;
	if (dispatch->item_block_list.tail) {
		block = pod_container_of(dispatch->item_block_list.tail, DispatchItemBlock_t, m_listnode);
		if (dispatch->items_pop_idx < sizeof(block->m_items) / sizeof(block->m_items[0])) {
			return &block->m_items[dispatch->items_pop_idx++];
		}
	}
	block = (DispatchItemBlock_t*)malloc(sizeof(DispatchItemBlock_t));
	if (!block) {
		return NULL;
	}
	listPushNodeBack(&dispatch->item_block_list, &block->m_listnode);
	dispatch->items_pop_idx = 1;
	return &block->m_items[0];
}

/**************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

int regStringDispatch(Dispatch_t* dispatch, const char* str, DispatchNetCallback_t func) {
	HashtableNodeKey_t hkey;
	DispatchItem_t* item;
	HashtableNode_t* exist_node;
	hkey.ptr = str;
	exist_node = hashtableSearchKey(&dispatch->string_table, hkey);
	if (exist_node) {
		pod_container_of(exist_node, DispatchItem_t, m_hashnode)->func = func;
		return 1;
	}
	item = (DispatchItem_t*)malloc(sizeof(DispatchItem_t) + strlen(str) + 1);
	if (!item) {
		return 0;
	}
	item->m_hashnode.key.ptr = (void*)(item + 1);
	item->func = func;
	strcpy((char*)item->m_hashnode.key.ptr, str);
	if (hashtableInsertNode(&dispatch->string_table, &item->m_hashnode) != &item->m_hashnode) {
		free(item);
		return 0;
	}
	return 1;
}

int regNumberDispatch(Dispatch_t* dispatch, int cmd, DispatchNetCallback_t func) {
	HashtableNodeKey_t hkey;
	DispatchItem_t* item;
	HashtableNode_t* exist_node;
	hkey.i32 = cmd;
	exist_node = hashtableSearchKey(&dispatch->number_table, hkey);
	if (exist_node) {
		pod_container_of(exist_node, DispatchItem_t, m_hashnode)->func = func;
		return 1;
	}
	item = pop_new_item(dispatch);
	if (!item) {
		return 0;
	}
	item->m_hashnode.key.i32 = cmd;
	item->func = func;
	if (hashtableInsertNode(&dispatch->number_table, &item->m_hashnode) != &item->m_hashnode) {
		--dispatch->items_pop_idx;
		return 0;
	}
	return 1;
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
