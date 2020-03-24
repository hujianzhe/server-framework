#ifndef MQ_SESSION_H
#define	MQ_SESSION_H

#include "util/inc/component/channel.h"
#include "util/inc/sysapi/process.h"

struct MQCluster_t;

typedef struct Session_t {
	HashtableNode_t m_htnode;
	Channel_t* channel;
	Fiber_t* fiber;
	List_t fiber_cmdlist;
	int fiber_busy;
	int id;
	struct MQCluster_t* cluster;
} Session_t;

#define	channelSession(channel)	((Session_t*)((channel)->userdata))

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
