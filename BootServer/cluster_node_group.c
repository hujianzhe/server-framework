#include "cluster_node_group.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ClusterNodeGroup_t* newClusterNodeGroup(const char* name) {
	ClusterNodeGroup_t* grp = (ClusterNodeGroup_t*)malloc(sizeof(ClusterNodeGroup_t));
	if (!grp) {
		return NULL;
	}
	grp->name = strdup(name);
	if (!grp->name) {
		free(grp);
		return NULL;
	}
	grp->m_htnode.key.ptr = grp->name;
	rbtreeInit(&grp->weight_num_ring, rbtreeDefaultKeyCmpU32);
	rbtreeInit(&grp->consistent_hash_ring, rbtreeDefaultKeyCmpU32);
	dynarrInitZero(&grp->clsnds);
	grp->target_loopcnt = 0;
	grp->total_weight = 0;
	return grp;
}

int regClusterNodeToGroup(struct ClusterNodeGroup_t* grp, ClusterNode_t* clsnd) {
	int ret_ok;
	dynarrInsert(&grp->clsnds, grp->clsnds.len, clsnd, ret_ok);
	return ret_ok;
}

int regClusterNodeToGroupByHashKey(struct ClusterNodeGroup_t* grp, unsigned int hashkey, ClusterNode_t* clsnd) {
	RBTreeNode_t* exist_node;
	struct {
		RBTreeNode_t _;
		ClusterNode_t* clsnd;
	} *data;
	*(void**)&data = malloc(sizeof(*data));
	if (!data) {
		return 0;
	}
	data->_.key.u32 = hashkey;
	data->clsnd = clsnd;
	exist_node = rbtreeInsertNode(&grp->consistent_hash_ring, &data->_);
	if (exist_node != &data->_) {
		rbtreeReplaceNode(&grp->consistent_hash_ring, exist_node, &data->_);
		free(exist_node);
	}
	return 1;
}

int regClusterNodeToGroupByWeight(struct ClusterNodeGroup_t* grp, int weight, ClusterNode_t* clsnd) {
	RBTreeNode_t* exist_node;
	struct {
		RBTreeNode_t _;
		ClusterNode_t* clsnd;
	} *data;
	*(void**)&data = malloc(sizeof(*data));
	if (!data) {
		return 0;
	}
	if (weight <= 0) {
		weight = 1;
	}
	grp->total_weight += weight;
	data->_.key.u32 = grp->total_weight;
	data->clsnd = clsnd;
	exist_node = rbtreeInsertNode(&grp->weight_num_ring, &data->_);
	if (exist_node != &data->_) {
		rbtreeReplaceNode(&grp->weight_num_ring, exist_node, &data->_);
		free(exist_node);
	}
	return 1;
}

void delCluserNodeFromGroup(struct ClusterNodeGroup_t* grp, const char* clsnd_ident) {
	RBTreeNode_t* rbcur, *rbnext;
	struct {
		RBTreeNode_t _;
		ClusterNode_t* clsnd;
	} *data;
	size_t i;
	for (i = 0; i < grp->clsnds.len; ++i) {
		if (0 == strcmp(grp->clsnds.buf[i]->ident, clsnd_ident)) {
			break;
		}
	}
	if (i < grp->clsnds.len) {
		dynarrRemoveIdx(&grp->clsnds, i);
	}
	for (rbcur = rbtreeFirstNode(&grp->consistent_hash_ring); rbcur; rbcur = rbnext) {
		rbnext = rbtreeNextNode(rbcur);
		*(void**)&data = rbcur;
		if (0 == strcmp(clsnd_ident, data->clsnd->ident)) {
			rbtreeRemoveNode(&grp->consistent_hash_ring, rbcur);
			free(data);
		}
	}
	for (rbcur = rbtreeFirstNode(&grp->weight_num_ring); rbcur; rbcur = rbnext) {
		rbnext = rbtreeNextNode(rbcur);
		*(void**)&data = rbcur;
		if (0 == strcmp(clsnd_ident, data->clsnd->ident)) {
			rbtreeRemoveNode(&grp->weight_num_ring, rbcur);
			free(data);
		}
	}
}

void freeClusterNodeGroup(struct ClusterNodeGroup_t* grp) {
	RBTreeNode_t* tcur, *tnext;
	for (tcur = rbtreeFirstNode(&grp->weight_num_ring); tcur; tcur = tnext) {
		tnext = rbtreeNextNode(tcur);
		rbtreeRemoveNode(&grp->weight_num_ring, tcur);
		free(tcur);
	}
	for (tcur = rbtreeFirstNode(&grp->consistent_hash_ring); tcur; tcur = tnext) {
		tnext = rbtreeNextNode(tcur);
		rbtreeRemoveNode(&grp->consistent_hash_ring, tcur);
		free(tcur);
	}
	dynarrFreeMemory(&grp->clsnds);
	free((void*)grp->name);
	free(grp);
}

#ifdef __cplusplus
}
#endif
