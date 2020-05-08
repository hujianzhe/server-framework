#include "../BootServer/global.h"
#include "mq_cmd.h"
#include "mq_cluster.h"
#include <string.h>

List_t g_ClusterList;
Hashtable_t g_ClusterGroupTable;
static HashtableNode_t* s_ClusterGroupBulk[32];
static int __keycmp(const void* node_key, const void* key) { return strcmp((const char*)node_key, (const char*)key); }
static unsigned int __keyhash(const void* key) { return hashBKDR((const char*)key); }

int initClusterTable(void) {
	hashtableInit(&g_ClusterGroupTable, s_ClusterGroupBulk, sizeof(s_ClusterGroupBulk) / sizeof(s_ClusterGroupBulk[0]), __keycmp, __keyhash);
	listInit(&g_ClusterList);
	return 1;
}

Cluster_t* newCluster(void) {
	Cluster_t* cluster = (Cluster_t*)malloc(sizeof(Cluster_t));
	if (cluster) {
		initSession(&cluster->session);
		cluster->session.usertype = SESSION_TYPE_CLUSTER;
	}
	return cluster;
}

void freeCluster(Cluster_t* cluster) {
	free(cluster);
}

ClusterGroup_t* getClusterGroup(const char* name) {
	HashtableNode_t* htnode = hashtableSearchKey(&g_ClusterGroupTable, name);
	return htnode ? pod_container_of(htnode, ClusterGroup_t, m_htnode) : NULL;
}

Cluster_t* getCluster(const char* name, const IPString_t ip, unsigned short port) {
	ClusterGroup_t* grp = getClusterGroup(name);
	if (grp) {
		ListNode_t* cur;
		for (cur = grp->clusterlist.head; cur; cur = cur->next) {
			Cluster_t* exist_cluster = pod_container_of(cur, Cluster_t, m_grp_listnode);
			if (!strcmp(exist_cluster->ip, ip) && exist_cluster->port == port) {
				return exist_cluster;
			}
		}
	}
	return NULL;
}

int regCluster(const char* name, Cluster_t* cluster) {
	ClusterGroup_t* item;
	HashtableNode_t* htnode;
	if (cluster->session.has_reg) {
		return 1;
	}
	htnode = hashtableSearchKey(&g_ClusterGroupTable, name);
	if (htnode) {
		item = pod_container_of(htnode, ClusterGroup_t, m_htnode);
	}
	else {
		item = (ClusterGroup_t*)malloc(sizeof(ClusterGroup_t));
		if (!item)
			return 0;
		item->m_htnode.key = strdup(name);
		if (!item->m_htnode.key)
			return 0;
		item->clusterlistcnt = 0;
		listInit(&item->clusterlist);
		hashtableInsertNode(&g_ClusterGroupTable, &item->m_htnode);
	}
	cluster->name = (const char*)item->m_htnode.key;
	cluster->grp = item;
	item->clusterlistcnt++;
	listPushNodeBack(&item->clusterlist, &cluster->m_grp_listnode);
	listPushNodeBack(&g_ClusterList, &cluster->m_listnode);
	cluster->session.has_reg = 1;
	return 1;
}

void unregCluster(Cluster_t* cluster) {
	if (cluster->session.has_reg) {
		ClusterGroup_t* item = cluster->grp;
		listRemoveNode(&item->clusterlist, &cluster->m_grp_listnode);
		item->clusterlistcnt--;
		if (!item->clusterlist.head) {
			hashtableRemoveNode(&g_ClusterGroupTable, &item->m_htnode);
			free((void*)item->m_htnode.key);
			free(item);
		}
		listRemoveNode(&g_ClusterList, &cluster->m_listnode);
		cluster->session.has_reg = 0;
	}
}

void freeClusterTable(void) {
	HashtableNode_t* curhtnode, *nexthtnode;
	for (curhtnode = hashtableFirstNode(&g_ClusterGroupTable); curhtnode; curhtnode = nexthtnode) {
		ListNode_t* curlistnode, *nextlistnode;
		ClusterGroup_t* item = pod_container_of(curhtnode, ClusterGroup_t, m_htnode);
		nexthtnode = hashtableNextNode(curhtnode);
		for (curlistnode = item->clusterlist.head; curlistnode; curlistnode = nextlistnode) {
			Cluster_t* cluster = pod_container_of(curlistnode, Cluster_t, m_grp_listnode);
			nextlistnode = curlistnode->next;
			freeCluster(cluster);
		}
		free((void*)item->m_htnode.key);
		free(item);
	}
	initClusterTable();
}