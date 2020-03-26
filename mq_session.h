#ifndef MQ_SESSION_H
#define	MQ_SESSION_H

#include "util/inc/component/channel.h"
#include "util/inc/sysapi/process.h"

struct MQRecvMsg_t;
struct MQCluster_t;

typedef struct RpcItem_t {
	RBTreeNode_t m_treenode;
	int id;
	long long timestamp_msec;
	long long timeout_msec;
	struct MQRecvMsg_t* ret_msg;
} RpcItem_t;

typedef struct Session_t {
	HashtableNode_t m_htnode;
	Channel_t* channel;
	int id;
	struct MQCluster_t* cluster;
	struct {
		Fiber_t* fiber;
		Fiber_t* sche_fiber;
		RBTree_t fiber_reg_rpc_tree;
		struct MQRecvMsg_t* fiber_new_msg;
		const void* fiber_net_disconnect_cmd;
	};
} Session_t;

#define	channelSession(channel)	((channel)->userdata)

int initSessionTable(void);
int allocSessionId(void);
Session_t* newSession(void);
Session_t* getSession(int id);
void regSession(int id, Session_t* session);
void unregSession(Session_t* session);

RpcItem_t* regSessionRpc(Session_t* session, int rpcid, long long timeout_msec);
RpcItem_t* existSessionRpc(Session_t* session, int rpcid);

void freeSession(Session_t* session);
void freeSessionTable(void);

void sessionBindChannel(Session_t* session, Channel_t* channel);
Channel_t* sessionUnbindChannel(Session_t* session);

#endif // !MQ_SESSION_H
