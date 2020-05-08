#ifndef CONSISTENT_HASH_CLUSTER_H
#define	CONSISTENT_HASH_CLUSTER_H

void consistenthashInit(void);
void consistenthashReg(unsigned int key, void* value);
void* consistenthashSelect(unsigned int key);
void consistenthashDel(void* value);
void consistenthashDelKey(unsigned int key);
void consistenthashFree(void);

#endif // !CONSISTENT_HASH_CLUSTER_H
