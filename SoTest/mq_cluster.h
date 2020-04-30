#ifndef MQ_CLUSTER_H
#define	MQ_CLUSTER_H

#include "../BootServer/util/inc/component/channel.h"

extern List_t g_ClusterList;
extern Hashtable_t g_ClusterTable;

struct Session_t;

typedef struct Cluster_t {
	ListNode_t m_reg_listnode;
	ListNode_t m_reg_htlistnode;
	void* m_reg_item;
	struct Session_t* session;
	const char* name;
	IPString_t ip;
	unsigned short port;
} Cluster_t;

#define	sessionCluster(session)		((session)->userdata)

int initClusterTable(void);
Cluster_t* getCluster(const char* name, const IPString_t ip, unsigned short port);
int regCluster(const char* name, Cluster_t* cluster);
void unregCluster(Cluster_t* cluster);
void freeClusterTable(void);

void clusterBindSession(Cluster_t* cluster, struct Session_t* session);
struct Session_t* clusterUnbindSession(Cluster_t* cluster);

#endif // !MQ_CLUSTER_H
