#ifndef CLUSTER_H
#define	CLUSTER_H

#include "util/inc/crt/consistent_hash.h"
#include "cluster_node.h"

typedef struct ClusterNodeGroup_t {
	HashtableNode_t m_htnode;
	List_t nodelist;
	unsigned int nodelistcnt;
	ConsistentHash_t consistent_hash;
	unsigned int target_loopcnt;
} ClusterNodeGroup_t;

#define	CLUSTER_TARGET_USE_HASH_MOD			1
#define	CLUSTER_TARGET_USE_HASH_RING		2
#define	CLUSTER_TARGET_USE_ROUND_ROBIN		3
#define	CLUSTER_TARGET_USE_WEIGHT_RANDOM	4
#define	CLUSTER_TARGET_USE_CONNECT_NUM		5
#define	CLUSTER_TARGET_USE_WEIGHT_MIN		6
#define	CLUSTER_TARGET_USE_WEIGHT_MAX		7

struct ClusterTable_t;
extern struct ClusterTable_t* g_ClusterTable;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport struct ClusterTable_t* ptr_g_ClusterTable(void);
__declspec_dllexport void set_g_ClusterTable(struct ClusterTable_t* t);

__declspec_dllexport struct ClusterTable_t* newClusterTable(void);
__declspec_dllexport ClusterNodeGroup_t* getClusterNodeGroup(struct ClusterTable_t* t, const char* name);
__declspec_dllexport ClusterNode_t* getClusterNodeFromGroup(ClusterNodeGroup_t* grp, int socktype, const IPString_t ip, unsigned short port);
__declspec_dllexport ClusterNode_t* getClusterNode(struct ClusterTable_t* t, int socktype, const IPString_t ip, unsigned short port);
__declspec_dllexport List_t* getClusterNodeList(struct ClusterTable_t* t);
__declspec_dllexport int regClusterNode(struct ClusterTable_t* t, const char* name, ClusterNode_t* clsnd);
__declspec_dllexport void unregClusterNode(struct ClusterTable_t* t, ClusterNode_t* clsnd);
__declspec_dllexport void freeClusterTable(struct ClusterTable_t* t);

__declspec_dllexport ClusterNode_t* targetClusterNode(ClusterNodeGroup_t* grp, int mode, unsigned int key);
__declspec_dllexport ClusterNode_t* targetClusterNodeByIp(ClusterNodeGroup_t* grp, const IPString_t ip, int mode, unsigned int key);
__declspec_dllexport void broadcastClusterGroup(ClusterNodeGroup_t* grp, const Iobuf_t iov[], unsigned int iovcnt);
__declspec_dllexport void broadcastClusterTable(struct ClusterTable_t* t, const Iobuf_t iov[], unsigned int iovcnt);

#ifdef __cplusplus
}
#endif

#endif
