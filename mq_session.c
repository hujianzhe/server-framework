#include "global.h"
#include "mq_session.h"

Hashtable_t g_SessionTable;
static HashtableNode_t* s_SessionBulk[1024];
static Atom32_t CHANNEL_SESSION_ID = 0;
static int __keycmp(const void* node_key, const void* key) { return node_key != key; }
static unsigned int __keyhash(const void* key) { return (ptrlen_t)key; }

static int __keycmp2(const void* node_key, const void* key) {
	return ((int)(size_t)node_key) - (int)((size_t)key);
}

int initSessionTable(void) {
	hashtableInit(&g_SessionTable, s_SessionBulk, sizeof(s_SessionBulk) / sizeof(s_SessionBulk[0]), __keycmp, __keyhash);
	return 1;
}

int allocSessionId(void) {
	int session_id;
	do {
		session_id = _xadd32(&CHANNEL_SESSION_ID, 1) + 1;
	} while (0 == session_id);
	return session_id;
}

Session_t* newSession(void) {
	Session_t* session = (Session_t*)malloc(sizeof(Session_t));
	if (session) {
		session->cluster = NULL;
		session->fiber = NULL;
		session->sche_fiber = NULL;
		session->fiber_new_msg = NULL;
		session->fiber_net_disconnect_cmd = NULL;
		session->fiber_msg_handler = NULL;
		rbtreeInit(&session->fiber_reg_rpc_tree, __keycmp2);
	}
	return session;
}

Session_t* getSession(int id) {
	HashtableNode_t* htnode = hashtableSearchKey(&g_SessionTable, (void*)(ptrlen_t)id);
	return htnode ? pod_container_of(htnode, Session_t, m_htnode) : NULL;
}

void regSession(int id, Session_t* session) {
	session->id = id;
	session->m_htnode.key = (void*)(ptrlen_t)id;
	hashtableReplaceNode(hashtableInsertNode(&g_SessionTable, &session->m_htnode), &session->m_htnode);
}

void unregSession(Session_t* session) {
	hashtableRemoveNode(&g_SessionTable, &session->m_htnode);
}

static RpcItem_t* regSessionRpc(Session_t* session, int rpcid, long long timeout_msec) {
	RpcItem_t* item = (RpcItem_t*)malloc(sizeof(RpcItem_t));
	if (item) {
		RBTreeNode_t* exist_node;
		item->m_treenode.key = (const void*)(size_t)rpcid;
		exist_node = rbtreeInsertNode(&session->fiber_reg_rpc_tree, &item->m_treenode);
		if (exist_node != &item->m_treenode) {
			free(item);
			return NULL;
		}
		item->id = rpcid;
		item->timestamp_msec = gmtimeMillisecond();
		item->timeout_msec = timeout_msec;
		item->ret_msg = NULL;
	}
	return item;
}

RpcItem_t* sessionExistRpc(Session_t* session, int rpcid) {
	RBTreeNode_t* node = rbtreeSearchKey(&session->fiber_reg_rpc_tree, (const void*)(size_t)rpcid);
	return node ? pod_container_of(node, RpcItem_t, m_treenode) : NULL;
}

RpcItem_t* sessionRpcWaitReturn(Session_t* session, int rpcid, long long timeout_msec) {
	RpcItem_t* rpc_item = regSessionRpc(session, rpcid, timeout_msec);
	if (rpc_item) {
		fiberSwitch(session->fiber, session->sche_fiber);
		while (session->fiber_new_msg) {
			void* msg = session->fiber_new_msg;
			session->fiber_new_msg = NULL;
			session->fiber_msg_handler(msg);
			if (session->fiber_net_disconnect_cmd) {
				break;
			}
			fiberSwitch(session->fiber, session->sche_fiber);
		}
		rbtreeRemoveNode(&session->fiber_reg_rpc_tree, &rpc_item->m_treenode);
	}
	return rpc_item;
}

int sessionRpcReturnSwitch(Session_t* session, int rpcid, void* ret_msg) {
	RpcItem_t* item = sessionExistRpc(session, rpcid);
	if (item) {
		item->ret_msg = ret_msg;
		fiberSwitch(session->sche_fiber, session->fiber);
		return 1;
	}
	return 0;
}

void sessionRpcMessageHandleSwitch(Session_t* session, void* new_msg) {
	session->fiber_new_msg = new_msg;
	fiberSwitch(session->sche_fiber, session->fiber);
}

void sessionRpcDisconnectHandleSwitch(Session_t* session, void* disconnect_cmd) {
	session->fiber_net_disconnect_cmd = disconnect_cmd;
	fiberSwitch(session->sche_fiber, session->fiber);
}

void sessionFiberProcEntry(Fiber_t* fiber) {
	Session_t* session = (Session_t*)fiber->arg;
	while (1) {
		if (session->fiber_new_msg) {
			void* msg = session->fiber_new_msg;
			session->fiber_new_msg = NULL;
			session->fiber_msg_handler(msg);
		}
		if (session->fiber_net_disconnect_cmd) {
			void* msg = session->fiber_net_disconnect_cmd;
			session->fiber_net_disconnect_cmd = NULL;
			session->fiber_msg_handler(msg);
		}
		fiberSwitch(fiber, session->sche_fiber);
	}
	fiberSwitch(fiber, session->sche_fiber);
}

void freeSession(Session_t* session) {
	if (session->fiber) {
		fiberFree(session->fiber);
	}
	free(session);
}

void freeSessionTable(void) {
	HashtableNode_t* htcur, *htnext;
	for (htcur = hashtableFirstNode(&g_SessionTable); htcur; htcur = htnext) {
		Session_t* session = pod_container_of(htcur, Session_t, m_htnode);
		htnext = hashtableNextNode(htcur);
		free(session);
	}
	hashtableInit(&g_SessionTable, s_SessionBulk, sizeof(s_SessionBulk) / sizeof(s_SessionBulk[0]), __keycmp, __keyhash);
}

void sessionBindChannel(Session_t* session, Channel_t* channel) {
	session->channel = channel;
	channelSession(channel) = session;
}

Channel_t* sessionUnbindChannel(Session_t* session) {
	if (session) {
		Channel_t* channel = session->channel;
		if (channel) {
			channelSession(channel) = NULL;
		}
		session->channel = NULL;
		return channel;
	}
	return NULL;
}