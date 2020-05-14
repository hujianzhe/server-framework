#ifndef CLUSTER_H
#define	CLUSTER_H

#include "util/inc/component/consistent_hash.h"
#include "session_struct.h"

typedef struct ClusterGroup_t {
	HashtableNode_t m_htnode;
	List_t clusterlist;
	size_t clusterlistcnt;
	ConsistentHash_t consistent_hash;
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
} Cluster_t;

extern Cluster_t* g_ClusterSelf;
extern List_t g_ClusterList;
extern Hashtable_t g_ClusterGroupTable;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll List_t* ptr_g_ClusterList(void);
__declspec_dll Hashtable_t* ptr_g_ClusterGroupTable(void);
__declspec_dll Cluster_t* ptr_g_ClusterSelf(void);

int initClusterTable(void);
__declspec_dll Cluster_t* newCluster(void);
__declspec_dll void freeCluster(Cluster_t* cluster);
__declspec_dll ClusterGroup_t* getClusterGroup(const char* name);
__declspec_dll Cluster_t* getCluster(const char* name, const IPString_t ip, unsigned short port);
__declspec_dll int regCluster(const char* name, Cluster_t* cluster);
__declspec_dll void unregCluster(Cluster_t* cluster);
void freeClusterTable(void);

#ifdef __cplusplus
}
#endif

#endif // !CLUSTER_H
