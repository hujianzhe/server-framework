#ifndef MQ_SESSION_H
#define	MQ_SESSION_H

#include "util/inc/component/channel.h"
#include "util/inc/sysapi/process.h"

struct MQRecvMsg_t;
struct MQCluster_t;

typedef struct Session_t {
	HashtableNode_t m_htnode;
	Channel_t* channel;
	int id;
	struct MQCluster_t* cluster;
	struct {
		Fiber_t* fiber;
		Fiber_t* sche_fiber;
		List_t fiber_cmdlist;
		RBTree_t fiber_reg_rpc_tree;
		struct MQRecvMsg_t* new_msg_when_fiber_busy;
		unsigned char* fiber_return_data;
		unsigned int fiber_return_datalen;
		int fiber_busy;
		long long fiber_wait_timestamp_msec;
		long long fiber_wait_timeout_msec;
	};
} Session_t;

#define	channelSession(channel)	((channel)->userdata)

int initSessionTable(void);
int allocSessionId(void);
Session_t* newSession(void);
Session_t* getSession(int id);
void regSession(int id, Session_t* session);
void unregSession(Session_t* session);

Session_t* regSessionRpc(Session_t* session, int cmd);
int existAndDeleteSessionRpc(Session_t* session, int cmd);
Session_t* saveSessionReturnData(Session_t* session, const void* data, unsigned int len);
void freeSessionReturnData(Session_t* session);

void freeSession(Session_t* session);
void freeSessionTable(void);

void sessionBindChannel(Session_t* session, Channel_t* channel);
Channel_t* sessionUnbindChannel(Session_t* session);

#endif // !MQ_SESSION_H
