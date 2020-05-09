#ifndef MQ_CLUSTER_H
#define	MQ_CLUSTER_H

#include "../BootServer/util/inc/component/consistent_hash.h"
#include "../BootServer/session_struct.h"

extern List_t g_ClusterList;
extern Hashtable_t g_ClusterGroupTable;

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
	IPString_t ip;
	unsigned short port;
} Cluster_t;

int initClusterTable(void);
Cluster_t* newCluster(void);
void freeCluster(Cluster_t* cluster);
ClusterGroup_t* getClusterGroup(const char* name);
Cluster_t* getCluster(const char* name, const IPString_t ip, unsigned short port);
int regCluster(const char* name, Cluster_t* cluster);
void unregCluster(Cluster_t* cluster);
void freeClusterTable(void);

#endif // !MQ_CLUSTER_H
