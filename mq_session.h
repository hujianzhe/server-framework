#ifndef MQ_SESSION_H
#define	MQ_SESSION_H

#include "util/inc/component/channel.h"
#include "util/inc/component/rbtimer.h"
#include "util/inc/sysapi/process.h"

struct Cluster_t;

typedef struct Session_t {
	HashtableNode_t m_htnode;
	Channel_t* channel;
	int id;
	struct Cluster_t* cluster;
	RpcFiberCore_t* f_rpc;
	RpcAsyncCore_t* a_rpc;
} Session_t;

#define	channelSession(channel)	((channel)->userdata)

int initSessionTable(void);
int allocSessionId(void);
Session_t* newSession(void);
Session_t* getSession(int id);
void regSession(int id, Session_t* session);
void unregSession(Session_t* session);
void freeSession(Session_t* session);
void freeSessionTable(void);

void sessionBindChannel(Session_t* session, Channel_t* channel);
Channel_t* sessionUnbindChannel(Session_t* session);

#endif // !MQ_SESSION_H
