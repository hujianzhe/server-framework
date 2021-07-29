#ifndef CLUSTER_H
#define	CLUSTER_H

#include "cluster_node.h"
#include "util/inc/crt/dynarr.h"
#include "util/inc/datastruct/rbtree.h"

#define	CLUSTER_TARGET_USE_HASH_MOD			1
#define	CLUSTER_TARGET_USE_HASH_RING		2
#define	CLUSTER_TARGET_USE_ROUND_ROBIN		3
#define	CLUSTER_TARGET_USE_WEIGHT_RANDOM	4
#define	CLUSTER_TARGET_USE_WEIGHT_MIN		6
#define	CLUSTER_TARGET_USE_WEIGHT_MAX		7
#define	CLUSTER_TARGET_USE_RANDOM			8

struct ClusterTable_t;
struct TaskThread_t;

typedef struct ClusterNodeGroup_t {
	HashtableNode_t m_htnode;
	const char* name;
	RBTree_t consistent_hash_ring;
	DynArr_t(ClusterNode_t*) clsnds;
	unsigned int target_loopcnt;
} ClusterNodeGroup_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport struct ClusterTable_t* newClusterTable(void);
__declspec_dllexport ClusterNode_t* getClusterNodeById(struct ClusterTable_t* t, int clsnd_id);
__declspec_dllexport List_t* getClusterNodeList(struct ClusterTable_t* t);
__declspec_dllexport void freeClusterTable(struct ClusterTable_t* t);

struct ClusterNodeGroup_t* newClusterNodeGroup(const char* name);
int regClusterNodeToGroup(struct ClusterNodeGroup_t* grp, ClusterNode_t* clsnd);
void freeClusterNodeGroup(struct ClusterNodeGroup_t* grp);
void replaceClusterNodeGroup(struct ClusterTable_t* t, struct ClusterNodeGroup_t** grps, size_t grp_cnt);

__declspec_dllexport ClusterNode_t* targetClusterNode(struct ClusterTable_t* t, const char* grp_name, int mode, unsigned int key);
__declspec_dllexport void broadcastClusterGroup(struct TaskThread_t* thrd, struct ClusterNodeGroup_t* grp, const Iobuf_t iov[], unsigned int iovcnt);
__declspec_dllexport void broadcastClusterTable(struct TaskThread_t* thrd, struct ClusterTable_t* t, const Iobuf_t iov[], unsigned int iovcnt);

#ifdef __cplusplus
}
#endif

#endif
