#ifndef CLUSTER_H
#define	CLUSTER_H

#include "cluster_node.h"

#define	CLUSTER_TARGET_USE_HASH_MOD			1
#define	CLUSTER_TARGET_USE_HASH_RING		2
#define	CLUSTER_TARGET_USE_ROUND_ROBIN		3
#define	CLUSTER_TARGET_USE_WEIGHT_RANDOM	4
#define	CLUSTER_TARGET_USE_CONNECT_NUM		5
#define	CLUSTER_TARGET_USE_WEIGHT_MIN		6
#define	CLUSTER_TARGET_USE_WEIGHT_MAX		7
#define	CLUSTER_TARGET_USE_RANDOM			8

struct ClusterTable_t;
struct ClusterNodeGroup_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport struct ClusterTable_t* newClusterTable(void);
__declspec_dllexport struct ClusterNodeGroup_t* getClusterNodeGroup(struct ClusterTable_t* t, const char* name);
__declspec_dllexport ClusterNode_t* getClusterNodeFromGroup(struct ClusterNodeGroup_t* grp, int socktype, const IPString_t ip, unsigned short port);
__declspec_dllexport ClusterNode_t* getClusterNode(struct ClusterTable_t* t, int socktype, const IPString_t ip, unsigned short port);
__declspec_dllexport ClusterNode_t* getClusterNodeById(struct ClusterTable_t* t, int clsnd_id);
__declspec_dllexport List_t* getClusterNodeList(struct ClusterTable_t* t);
int regClusterNode(struct ClusterTable_t* t, const char* name, ClusterNode_t* clsnd);
void unregClusterNode(struct ClusterTable_t* t, ClusterNode_t* clsnd);
__declspec_dllexport void freeClusterTable(struct ClusterTable_t* t);

__declspec_dllexport ClusterNode_t* targetClusterNode(struct ClusterNodeGroup_t* grp, int mode, unsigned int key);
__declspec_dllexport ClusterNode_t* targetClusterNodeByIp(struct ClusterNodeGroup_t* grp, const IPString_t ip, int mode, unsigned int key);
__declspec_dllexport void broadcastClusterGroup(struct ClusterNodeGroup_t* grp, const Iobuf_t iov[], unsigned int iovcnt);
__declspec_dllexport void broadcastClusterTable(struct ClusterTable_t* t, const Iobuf_t iov[], unsigned int iovcnt);

#ifdef __cplusplus
}
#endif

#endif
