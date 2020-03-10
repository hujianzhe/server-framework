#ifndef MQ_CLUSTER_H
#define	MQ_CLUSTER_H

#include "util/inc/component/channel.h"

struct Session_t;

typedef struct MQCluster_t {
	ListNode_t m_reg_listnode;
	ListNode_t m_reg_htlistnode;
	void* m_reg_item;
	struct Session_t* session;
	const char* name;
	IPString_t ip;
	unsigned short port;
} MQCluster_t;

int initClusterTable(void);
MQCluster_t* getCluster(const char* name, const IPString_t ip, unsigned short port);
int regCluster(const char* name, MQCluster_t* cluster);
void unregCluster(MQCluster_t* cluster);
void freeClusterTable(void);

void clusterBindSession(MQCluster_t* cluster, struct Session_t* session);
struct Session_t* clusterUnbindSession(MQCluster_t* cluster);

#endif // !MQ_CLUSTER_H
