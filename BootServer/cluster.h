#ifndef BOOT_SERVER_CLUSTER_H
#define	BOOT_SERVER_CLUSTER_H

#include "cluster_node.h"
#include "util/inc/crt/dynarr.h"
#include "util/inc/datastruct/rbtree.h"

enum {
	CLUSTER_TARGET_USE_HASH_MOD	= 1,
	CLUSTER_TARGET_USE_HASH_RING = 2,
	CLUSTER_TARGET_USE_ROUND_ROBIN = 3,
	CLUSTER_TARGET_USE_WEIGHT_RANDOM = 4,
	CLUSTER_TARGET_USE_RANDOM = 5
};

struct ClusterTable_t;
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
__declspec_dll void freeClusterNodeGroup(struct ClusterNodeGroup_t* grp);

__declspec_dll struct ClusterTable_t* newClusterTable(void);
__declspec_dll ClusterNode_t* getClusterNodeById(struct ClusterTable_t* t, int clsnd_id);
__declspec_dll struct ClusterNodeGroup_t* getClusterNodeGroup(struct ClusterTable_t* t, const char* grp_name); /* maybe dangerous */
__declspec_dll void getClusterNodes(struct ClusterTable_t* t, DynArrClusterNodePtr_t* v);
__declspec_dll void getClusterGroupNodes(struct ClusterTable_t* t, const char* grp_name, DynArrClusterNodePtr_t* v);
__declspec_dll void clearClusterNodeGroup(struct ClusterTable_t* t);
__declspec_dll void replaceClusterNodeGroup(struct ClusterTable_t* t, struct ClusterNodeGroup_t* grp);
__declspec_dll void freeClusterTable(struct ClusterTable_t* t);
__declspec_dll ClusterNode_t* targetClusterNode(struct ClusterTable_t* t, const char* grp_name, int mode, unsigned int key);
__declspec_dll void broadcastClusterGroup(struct ClusterTable_t* t, const char* grp_name, const Iobuf_t iov[], unsigned int iovcnt);
__declspec_dll void broadcastClusterTable(struct ClusterTable_t* t, const Iobuf_t iov[], unsigned int iovcnt);

#ifdef __cplusplus
}
#endif

#endif
