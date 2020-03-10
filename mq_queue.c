#include "global.h"
#include "mq_queue.h"
#include <string.h>

Hashtable_t g_QueueTable;
static HashtableNode_t* s_QueueBulks[32];
static int __keycmp(const void* node_key, const void* key) { return strcmp((const char*)node_key, (const char*)key); }
static unsigned int __keyhash(const void* key) { return hashBKDR((const char*)key); }

int initQueueTable(void) {
	hashtableInit(&g_QueueTable, s_QueueBulks, sizeof(s_QueueBulks) / sizeof(s_QueueBulks[0]), __keycmp, __keyhash);
	return 1;
}

MQQueue_t* getQueue(const char* name) {
	HashtableNode_t* htnode = hashtableSearchKey(&g_QueueTable, name);
	return htnode ? pod_container_of(htnode, MQQueue_t, m_htnode) : NULL;
}

void regQueue(const char* name, MQQueue_t* queue) {
	if (queue->name != name) {
		strncpy(queue->name, name, sizeof(queue->name) - 1);
	}
	queue->name[sizeof(queue->name) - 1] = 0;
	queue->m_htnode.key = (void*)queue->name;
	hashtableReplaceNode(hashtableInsertNode(&g_QueueTable, &queue->m_htnode), &queue->m_htnode);
}

void freeQueueTable(void) {
	HashtableNode_t* htcur, *htnext;
	for (htcur = hashtableFirstNode(&g_QueueTable); htcur; htcur = htnext) {
		MQQueue_t* queue = pod_container_of(htcur, MQQueue_t, m_htnode);
		htnext = hashtableNextNode(htcur);
		free(queue);
	}
	hashtableInit(&g_QueueTable, s_QueueBulks, sizeof(s_QueueBulks) / sizeof(s_QueueBulks[0]), __keycmp, __keyhash);
}