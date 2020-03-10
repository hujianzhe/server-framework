#ifndef MQ_QUEUE_H
#define	MQ_QUEUE_H

#include "util/inc/component/channel.h"

typedef struct MQQueue_t {
	HashtableNode_t m_htnode;
	char name[256];
} MQQueue_t;

int initQueueTable(void);
MQQueue_t* getQueue(const char* name);
void regQueue(const char* name, MQQueue_t* queue);
void freeQueueTable(void);

#endif // !MQ_QUEUE_H
