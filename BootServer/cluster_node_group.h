#ifndef BOOT_SERVER_CLUSTER_NODE_GROUP_H
#define	BOOT_SERVER_CLUSTER_NODE_GROUP_H

#include "cluster_node.h"
#include "util/inc/datastruct/rbtree.h"
#include "util/inc/datastruct/hashtable.h"
#include "util/inc/crt/dynarr.h"

typedef DynArr_t(ClusterNode_t*) DynArrClusterNodePtr_t;

typedef struct ClusterNodeGroup_t {
	HashtableNode_t m_htnode;
	const char* name;
	RBTree_t weight_num_ring;
	RBTree_t consistent_hash_ring;
	DynArrClusterNodePtr_t clsnds;
	unsigned int target_loopcnt;
	unsigned int total_weight;
} ClusterNodeGroup_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll struct ClusterNodeGroup_t* newClusterNodeGroup(const char* name);
__declspec_dll int regClusterNodeToGroup(struct ClusterNodeGroup_t* grp, ClusterNode_t* clsnd);
__declspec_dll int regClusterNodeToGroupByHashKey(struct ClusterNodeGroup_t* grp, unsigned int hashkey, ClusterNode_t* clsnd);
__declspec_dll int regClusterNodeToGroupByWeight(struct ClusterNodeGroup_t* grp, int weight, ClusterNode_t* clsnd);
__declspec_dll void delCluserNodeFromGroup(struct ClusterNodeGroup_t* grp, int clsnd_id);
__declspec_dll void freeClusterNodeGroup(struct ClusterNodeGroup_t* grp);

#ifdef __cplusplus
}
#endif

#endif // !BOOT_SERVER_CLUSTER_NODE_GROUP_H
