#ifndef CLUSTER_H
#define	CLUSTER_H

#include "util/inc/component/consistent_hash.h"
#include "session_struct.h"

typedef struct ClusterGroup_t {
	HashtableNode_t m_htnode;
	List_t clusterlist;
	unsigned int clusterlistcnt;
	ConsistentHash_t consistent_hash;
	unsigned int target_loopcnt;
} ClusterGroup_t;

typedef struct Cluster_t {
	Session_t session;
	ListNode_t m_listnode;
	ListNode_t m_grp_listnode;
	ClusterGroup_t* grp;
	const char* name;
	int socktype;
	IPString_t ip;
	unsigned short port;
	unsigned int* hashkey;
	unsigned int hashkey_cnt;
	int weight_num;
	int connection_num;
} Cluster_t;

struct ClusterTable_t;

extern Cluster_t* g_ClusterSelf;
extern struct ClusterTable_t* g_ClusterTable;
extern int g_ClusterTableVersion;

#define	CLUSTER_TARGET_USE_HASH_MOD		1
#define	CLUSTER_TARGET_USE_HASH_RING	2
#define	CLUSTER_TARGET_USE_ROUND_ROBIN	3
#define	CLUSTER_TARGET_USE_WEIGHT_NUM	4
#define	CLUSTER_TARGET_USE_CONNECT_NUM	5

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport Cluster_t* getClusterSelf(void);
__declspec_dllexport void setClusterSelf(Cluster_t* cluster);
__declspec_dllexport struct ClusterTable_t* ptr_g_ClusterTable(void);
__declspec_dllexport void set_g_ClusterTable(struct ClusterTable_t* t);
__declspec_dllexport int getClusterTableVersion(void);
__declspec_dllexport void setClusterTableVersion(int version);

__declspec_dllexport Cluster_t* newCluster(int socktype, IPString_t ip, unsigned short port);
__declspec_dllexport void freeCluster(Cluster_t* cluster);
__declspec_dllexport Channel_t* clusterChannel(Cluster_t* cluster);
__declspec_dllexport unsigned int* reallocClusterHashKey(Cluster_t* cluster, unsigned int key_arraylen);

__declspec_dllexport struct ClusterTable_t* newClusterTable(void);
__declspec_dllexport ClusterGroup_t* getClusterGroup(struct ClusterTable_t* t, const char* name);
__declspec_dllexport Cluster_t* getCluster(struct ClusterTable_t* t, const char* name, const IPString_t ip, unsigned short port);
__declspec_dllexport List_t* getClusterList(struct ClusterTable_t* t);
__declspec_dllexport int regCluster(struct ClusterTable_t* t, const char* name, Cluster_t* cluster);
__declspec_dllexport void unregCluster(struct ClusterTable_t* t, Cluster_t* cluster);
__declspec_dllexport void freeClusterTable(struct ClusterTable_t* t);

__declspec_dllexport Cluster_t* targetCluster(ClusterGroup_t* grp, int mode, unsigned int key);


#ifdef __cplusplus
}
#endif

#endif // !CLUSTER_H
