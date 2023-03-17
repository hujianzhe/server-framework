#ifndef BOOT_SERVER_CLUSTER_H
#define	BOOT_SERVER_CLUSTER_H

#include "cluster_node_group.h"

enum {
	CLUSTER_TARGET_USE_HASH_MOD	= 1,
	CLUSTER_TARGET_USE_HASH_RING = 2,
	CLUSTER_TARGET_USE_ROUND_ROBIN = 3,
	CLUSTER_TARGET_USE_WEIGHT_RANDOM = 4,
	CLUSTER_TARGET_USE_RANDOM = 5,
	CLUSTER_TARGET_USE_FACTOR_MIN = 6,
	CLUSTER_TARGET_USE_FACTOR_MAX = 7
};

struct ClusterTable_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll struct ClusterTable_t* newClusterTable(void);
__declspec_dll void freeClusterTable(struct ClusterTable_t* t);

__declspec_dll ClusterNode_t* getClusterNodeById(struct ClusterTable_t* t, const char* clsnd_ident);
__declspec_dll struct ClusterNodeGroup_t* getClusterNodeGroup(struct ClusterTable_t* t, const char* grp_name);
__declspec_dll void getClusterNodes(struct ClusterTable_t* t, DynArrClusterNodePtr_t* v);
__declspec_dll void getClusterGroupNodes(struct ClusterTable_t* t, const char* grp_name, DynArrClusterNodePtr_t* v);
__declspec_dll ClusterNode_t* targetClusterNode(struct ClusterTable_t* t, const char* grp_name, int mode, unsigned int key);

__declspec_dll void clearClusterNodeGroup(struct ClusterTable_t* t);
__declspec_dll void replaceClusterNodeGroup(struct ClusterTable_t* t, struct ClusterNodeGroup_t* grp);
__declspec_dll void inactiveClusterNode(struct ClusterTable_t* t, ClusterNode_t* clsnd);

__declspec_dll void broadcastClusterGroup(struct ClusterTable_t* t, const char* grp_name, const Iobuf_t iov[], unsigned int iovcnt);
__declspec_dll void broadcastClusterTable(struct ClusterTable_t* t, const Iobuf_t iov[], unsigned int iovcnt);

#ifdef __cplusplus
}
#endif

#endif
