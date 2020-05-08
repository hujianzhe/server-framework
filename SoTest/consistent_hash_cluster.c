#include "../BootServer/global.h"
#include "consistent_hash_cluster.h"

RBTree_t s_ConstHashCluster;
typedef struct VirtualNode_t {
	RBTreeNode_t m_treenode;
	void* value;
} VirtualNode_t;
static int __consthash_keycmp(const void* node_key, const void* key) {
	ssize_t res = (ssize_t)key - (ssize_t)node_key;
	if (res < 0)
		return -1;
	else if (res > 0)
		return 1;
	else
		return 0;
}

void consistenthashInit(void) {
	rbtreeInit(&s_ConstHashCluster, __consthash_keycmp);
}

void consistenthashReg(unsigned int key, void* value) {
	RBTreeNode_t* exist_node;
	VirtualNode_t* vc = (VirtualNode_t*)malloc(sizeof(VirtualNode_t));
	vc->m_treenode.key = (void*)(size_t)key;
	vc->value = value;
	exist_node = rbtreeInsertNode(&s_ConstHashCluster, &vc->m_treenode);
	if (exist_node != &vc->m_treenode) {
		rbtreeReplaceNode(exist_node, &vc->m_treenode);
		free(pod_container_of(exist_node, VirtualNode_t, m_treenode));
	}
}

void* consistenthashSelect(unsigned int key) {
	RBTreeNode_t* exist_node = rbtreeUpperBoundKey(&s_ConstHashCluster, (void*)(size_t)key);
	if (!exist_node) {
		exist_node = rbtreeFirstNode(&s_ConstHashCluster);
		if (!exist_node) {
			return NULL;
		}
	}
	return pod_container_of(exist_node, VirtualNode_t, m_treenode)->value;
}

void consistenthashDel(void* value) {
	RBTreeNode_t* cur, *next;
	for (cur = rbtreeFirstNode(&s_ConstHashCluster); cur; cur = next) {
		VirtualNode_t* vc = pod_container_of(cur, VirtualNode_t, m_treenode);
		next = rbtreeNextNode(cur);
		if (vc->value != value)
			continue;
		rbtreeRemoveNode(&s_ConstHashCluster, cur);
		free(vc);
	}
}

void consistenthashDelKey(unsigned int key) {
	RBTreeNode_t* exist_node = rbtreeRemoveKey(&s_ConstHashCluster, (void*)(size_t)key);
	if (exist_node) {
		free(pod_container_of(exist_node, VirtualNode_t, m_treenode));
	}
}

void consistenthashFree(void) {
	RBTreeNode_t* cur, *next;
	for (cur = rbtreeFirstNode(&s_ConstHashCluster); cur; cur = next) {
		VirtualNode_t* vc = pod_container_of(cur, VirtualNode_t, m_treenode);
		next = rbtreeNextNode(cur);
		rbtreeRemoveNode(&s_ConstHashCluster, cur);
		free(vc);
	}
}