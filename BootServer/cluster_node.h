#ifndef CLUSTER_NODE_H
#define	CLUSTER_NODE_H

#include "session_struct.h"

struct DataQueue_t;

enum {
	CLSND_STATUS_NORMAL = 0,
	CLSND_STATUS_INACTIVE = 1
};

typedef struct ClusterNode_t {
	Session_t session;
	ListNode_t m_listnode;
	HashtableNode_t m_id_htnode;
	int id;
	int socktype;
	IPString_t ip;
	unsigned short port;
	int connection_num;
	int status;
} ClusterNode_t;

#ifdef __cplusplus
extern "C" {
#endif

ClusterNode_t* newClusterNode(int id, int socktype, IPString_t ip, unsigned short port);
void freeClusterNode(ClusterNode_t* clsnd);
__declspec_dllexport Channel_t* connectClusterNode(ClusterNode_t* clsnd, struct DataQueue_t* dq);

#ifdef __cplusplus
}
#endif

#endif // !CLUSTER_NODE_H
