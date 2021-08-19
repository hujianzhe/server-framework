#ifndef CLUSTER_H
#define	CLUSTER_H

#include "cluster_node.h"
#include "util/inc/crt/dynarr.h"
#include "util/inc/datastruct/rbtree.h"

#define	CLUSTER_TARGET_USE_HASH_MOD			1
#define	CLUSTER_TARGET_USE_HASH_RING		2
#define	CLUSTER_TARGET_USE_ROUND_ROBIN		3
#define	CLUSTER_TARGET_USE_WEIGHT_RANDOM	4
#define	CLUSTER_TARGET_USE_RANDOM			5

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

__declspec_dllexport struct ClusterTable_t* newClusterTable(void);
__declspec_dllexport ClusterNode_t* getClusterNodeById(struct ClusterTable_t* t, int clsnd_id);
__declspec_dllexport void getClusterNodes(struct ClusterTable_t* t, DynArrClusterNodePtr_t* v);
__declspec_dllexport void getClusterGroupNodes(struct ClusterTable_t* t, const char* grp_name, DynArrClusterNodePtr_t* v);
__declspec_dllexport void freeClusterTable(struct ClusterTable_t* t);

struct ClusterNodeGroup_t* newClusterNodeGroup(const char* name);
int regClusterNodeToGroup(struct ClusterNodeGroup_t* grp, ClusterNode_t* clsnd);
void freeClusterNodeGroup(struct ClusterNodeGroup_t* grp);
void replaceClusterNodeGroup(struct ClusterTable_t* t, struct ClusterNodeGroup_t** grps, size_t grp_cnt);

__declspec_dllexport ClusterNode_t* targetClusterNode(struct ClusterTable_t* t, const char* grp_name, int mode, unsigned int key);
__declspec_dllexport void broadcastClusterGroup(struct DataQueue_t* dq, struct ClusterTable_t* t, const char* grp_name, const Iobuf_t iov[], unsigned int iovcnt);
__declspec_dllexport void broadcastClusterTable(struct DataQueue_t* dq, struct ClusterTable_t* t, const Iobuf_t iov[], unsigned int iovcnt);

#ifdef __cplusplus
}
#endif

#endif
