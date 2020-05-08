#ifndef CONSISTENT_HASH_CLUSTER_H
#define	CONSISTENT_HASH_CLUSTER_H

#include "mq_cluster.h"

void consistenthashInit(void);
void consistenthashReg(unsigned int key, Cluster_t* cluster);
Cluster_t* consistenthashSelect(unsigned int key);
void consistenthashDel(Cluster_t* cluster);
void consistenthashDelKey(unsigned int key);
void consistenthashFree(void);

#endif // !CONSISTENT_HASH_CLUSTER_H
