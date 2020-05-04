#ifndef MQ_CLUSTER_H
#define	MQ_CLUSTER_H

#include "../BootServer/session.h"

extern List_t g_ClusterList;
extern Hashtable_t g_ClusterTable;

typedef struct Cluster_t {
	Session_t session;
	ListNode_t m_reg_listnode;
	ListNode_t m_reg_htlistnode;
	void* m_reg_item;
	const char* name;
	IPString_t ip;
	unsigned short port;
} Cluster_t;

int initClusterTable(void);
Cluster_t* getCluster(const char* name, const IPString_t ip, unsigned short port);
int regCluster(const char* name, Cluster_t* cluster);
void unregCluster(Cluster_t* cluster);
void freeClusterTable(void);

Session_t* newSession(int type);
void freeSession(Session_t* session);

#endif // !MQ_CLUSTER_H
