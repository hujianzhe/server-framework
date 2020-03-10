#ifndef MQ_SESSION_H
#define	MQ_SESSION_H

#include "util/inc/component/channel.h"

struct MQCluster_t;

typedef struct Session_t {
	HashtableNode_t m_htnode;
	Channel_t* channel;
	int id;
	int type;
	struct MQCluster_t* cluster;
} Session_t;

int initSessionTable(void);
int allocSessionId(void);
Session_t* getSession(int id);
void regSession(int id, Session_t* session);
void unregSession(Session_t* session);
void freeSessionTable(void);

void sessionBindChannel(Session_t* session, Channel_t* channel);
Channel_t* sessionUnbindChannel(Session_t* session);

#endif // !MQ_SESSION_H
