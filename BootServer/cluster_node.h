#ifndef BOOT_SERVER_CLUSTER_NODE_H
#define	BOOT_SERVER_CLUSTER_NODE_H

#include "session_struct.h"

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

ClusterNode_t* newClusterNode(int id, int socktype, const IPString_t ip, unsigned short port);
void freeClusterNode(ClusterNode_t* clsnd);
__declspec_dll Channel_t* connectClusterNode(ClusterNode_t* clsnd);
__declspec_dll void clsndSendv(ClusterNode_t* clsnd, const Iobuf_t iov[], unsigned int iovcnt);

#ifdef __cplusplus
}
#endif

#endif // !CLUSTER_NODE_H
