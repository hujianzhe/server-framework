#ifndef CLUSTER_NODE_H
#define	CLUSTER_NODE_H

#include "session_struct.h"

struct ClusterNodeGroup_t;

typedef struct ClusterNode_t {
	Session_t session;
	ListNode_t m_listnode;
	ListNode_t m_grp_listnode;
	HashtableNode_t m_id_htnode;
	struct ClusterNodeGroup_t* grp;
	const char* name;
	int id;
	int socktype;
	IPString_t ip;
	unsigned short port;
	unsigned int* hashkey;
	unsigned int hashkey_cnt;
	int weight_num;
	int connection_num;
} ClusterNode_t;

#ifdef __cplusplus
extern "C" {
#endif

ClusterNode_t* newClusterNode(int id, int socktype, IPString_t ip, unsigned short port);
void freeClusterNode(ClusterNode_t* clsnd);
__declspec_dllexport Channel_t* connectClusterNode(ClusterNode_t* clsnd);
unsigned int* reallocClusterNodeHashKey(ClusterNode_t* clsnd, unsigned int key_arraylen);

#ifdef __cplusplus
}
#endif

#endif // !CLUSTER_NODE_H
