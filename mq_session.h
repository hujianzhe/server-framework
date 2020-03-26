#ifndef MQ_SESSION_H
#define	MQ_SESSION_H

#include "util/inc/component/channel.h"
#include "util/inc/sysapi/process.h"

struct MQCluster_t;

typedef struct RpcItem_t {
	RBTreeNode_t m_treenode;
	int id;
	long long timestamp_msec;
	long long timeout_msec;
	void* ret_msg;
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
		void* fiber_new_msg;
		void* fiber_net_disconnect_cmd;
		void(*fiber_msg_handler)(void*);
	};
} Session_t;

#define	channelSession(channel)	((channel)->userdata)

int initSessionTable(void);
int allocSessionId(void);
Session_t* newSession(void);
Session_t* getSession(int id);
void regSession(int id, Session_t* session);
void unregSession(Session_t* session);

RpcItem_t* sessionExistRpc(Session_t* session, int rpcid);
void sessionFreeRpc(Session_t* session, RpcItem_t* item);
RpcItem_t* sessionRpcWaitReturn(Session_t* session, int rpcid, long long timeout_msec);
int sessionRpcReturnSwitch(Session_t* session, int rpcid, void* ret_msg);
void sessionRpcMessageHandleSwitch(Session_t* session, void* new_msg);
void sessionRpcDisconnectHandleSwitch(Session_t* session, void* disconnect_cmd);
void sessionFiberProcEntry(Fiber_t* fiber);

void freeSession(Session_t* session);
void freeSessionTable(void);

void sessionBindChannel(Session_t* session, Channel_t* channel);
Channel_t* sessionUnbindChannel(Session_t* session);

#endif // !MQ_SESSION_H
