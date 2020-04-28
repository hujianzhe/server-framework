#ifndef SESSION_H
#define	SESSION_H

#include "util/inc/component/channel.h"

typedef struct Session_t {
	HashtableNode_t m_htnode;
	int has_reg;
	Channel_t* channel;
	int id;
	void* userdata;
	List_t rpc_itemlist;
} Session_t;

#define	channelSession(channel)		((channel)->userdata)
#define	channelSessionId(channel)	((channel)->userid32)

int initSessionTable(void);
int allocSessionId(void);
Session_t* newSession(void);
Session_t* getSession(int id);
void regSession(int id, Session_t* session);
Session_t* unregSession(Session_t* session);
void freeSession(Session_t* session);
void freeSessionTable(void);

void sessionBindChannel(Session_t* session, Channel_t* channel);
Channel_t* sessionUnbindChannel(Session_t* session);

#endif // !SESSION_H
