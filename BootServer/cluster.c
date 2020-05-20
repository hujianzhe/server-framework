#include "config.h"
#include "global.h"
#include "cluster.h"
#include <string.h>

Cluster_t* g_ClusterSelf;
List_t g_ClusterList;
Hashtable_t g_ClusterGroupTable;
static HashtableNode_t* s_ClusterGroupBulk[32];
static int __keycmp(const void* node_key, const void* key) { return strcmp((const char*)node_key, (const char*)key); }
static unsigned int __keyhash(const void* key) { return hashBKDR((const char*)key); }

static ClusterGroup_t* new_cluster_group(const char* name) {
	ClusterGroup_t* grp = (ClusterGroup_t*)malloc(sizeof(ClusterGroup_t));
	if (!grp)
		return NULL;
	grp->m_htnode.key = strdup(name);
	if (!grp->m_htnode.key) {
		free(grp);
		return NULL;
	}
	grp->clusterlistcnt = 0;
	listInit(&grp->clusterlist);
	consistenthashInit(&grp->consistent_hash);
	return grp;
}

static int cluster_reg_consistenthash(ClusterGroup_t* grp, Cluster_t* cluster) {
	int i;
	for (i = 0; i < cluster->key_arraylen; ++i) {
		if (!consistenthashReg(&grp->consistent_hash, cluster->key_array[i], cluster))
			break;
	}
	if (i != cluster->key_arraylen) {
		consistenthashDelValue(&grp->consistent_hash, cluster);
		return 0;
	}
	return 1;
}

static void free_cluster_group(ClusterGroup_t* grp) {
	free((void*)grp->m_htnode.key);
	consistenthashFree(&grp->consistent_hash);
	free(grp);
}

static void cluster_session_destroy(Session_t* session) {
	Cluster_t* cluster = pod_container_of(session, Cluster_t, session);
	unregCluster(cluster);
	freeCluster(cluster);
}

#ifdef __cplusplus
extern "C" {
#endif

List_t* ptr_g_ClusterList(void) { return &g_ClusterList; }
Hashtable_t* ptr_g_ClusterGroupTable(void) { return &g_ClusterGroupTable; }
Cluster_t* ptr_g_ClusterSelf(void) { return g_ClusterSelf; }

int initClusterTable(void) {
	hashtableInit(&g_ClusterGroupTable, s_ClusterGroupBulk, sizeof(s_ClusterGroupBulk) / sizeof(s_ClusterGroupBulk[0]), __keycmp, __keyhash);
	listInit(&g_ClusterList);
	return 1;
}

Cluster_t* newCluster(void) {
	Cluster_t* cluster = (Cluster_t*)malloc(sizeof(Cluster_t));
	if (cluster) {
		initSession(&cluster->session);
		cluster->session.persist = 1;
		cluster->session.destroy = cluster_session_destroy;
		cluster->grp = NULL;
		cluster->name = NULL;
		cluster->socktype = 0;
		cluster->ip[0] = 0;
		cluster->port = 0;
		cluster->key_array = NULL;
		cluster->key_arraylen = 0;
	}
	return cluster;
}

unsigned int* newClusterKeyArray(Cluster_t* cluster, unsigned int key_arraylen) {
	unsigned int* key_array = (unsigned int*)malloc(sizeof(cluster->key_array[0]) * key_arraylen);
	if (key_array) {
		int i;
		for (i = 0; i < key_arraylen; ++i) {
			cluster->key_array[i] = 0;
		}
		cluster->key_array = key_array;
		cluster->key_arraylen = key_arraylen;
	}
	return key_array;
}

void freeCluster(Cluster_t* cluster) {
	if (cluster) {
		free(cluster->key_array);
		free(cluster);
		if (cluster == g_ClusterSelf)
			g_ClusterSelf = NULL;
	}
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
	ClusterGroup_t* grp;
	HashtableNode_t* htnode;
	if (cluster->session.has_reg) {
		return 1;
	}
	htnode = hashtableSearchKey(&g_ClusterGroupTable, name);
	if (htnode) {
		grp = pod_container_of(htnode, ClusterGroup_t, m_htnode);
		if (!cluster_reg_consistenthash(grp, cluster)) {
			return 0;
		}
	}
	else {
		grp = new_cluster_group(name);
		if (!grp)
			return 0;
		if (!cluster_reg_consistenthash(grp, cluster)) {
			return 0;
		}
		hashtableInsertNode(&g_ClusterGroupTable, &grp->m_htnode);
	}
	cluster->name = (const char*)grp->m_htnode.key;
	cluster->grp = grp;
	grp->clusterlistcnt++;
	listPushNodeBack(&grp->clusterlist, &cluster->m_grp_listnode);
	listPushNodeBack(&g_ClusterList, &cluster->m_listnode);
	cluster->session.has_reg = 1;
	return 1;
}

void unregCluster(Cluster_t* cluster) {
	if (cluster->session.has_reg && cluster->grp) {
		ClusterGroup_t* grp = cluster->grp;
		listRemoveNode(&grp->clusterlist, &cluster->m_grp_listnode);
		grp->clusterlistcnt--;
		if (grp->clusterlist.head) {
			consistenthashDelValue(&grp->consistent_hash, cluster);
		}
		else {
			hashtableRemoveNode(&g_ClusterGroupTable, &grp->m_htnode);
			free_cluster_group(grp);
		}
		listRemoveNode(&g_ClusterList, &cluster->m_listnode);
		cluster->session.has_reg = 0;
		cluster->grp = NULL;
	}
}

void freeClusterTable(void) {
	HashtableNode_t* curhtnode, *nexthtnode;
	for (curhtnode = hashtableFirstNode(&g_ClusterGroupTable); curhtnode; curhtnode = nexthtnode) {
		ListNode_t* curlistnode, *nextlistnode;
		ClusterGroup_t* grp = pod_container_of(curhtnode, ClusterGroup_t, m_htnode);
		nexthtnode = hashtableNextNode(curhtnode);
		for (curlistnode = grp->clusterlist.head; curlistnode; curlistnode = nextlistnode) {
			Cluster_t* cluster = pod_container_of(curlistnode, Cluster_t, m_grp_listnode);
			nextlistnode = curlistnode->next;
			freeCluster(cluster);
		}
		free_cluster_group(grp);
	}
	freeCluster(g_ClusterSelf);
	initClusterTable();
}

Cluster_t* targetCluster(int mode, const char* name, unsigned int key) {
	ClusterGroup_t* grp = getClusterGroup(name);
	if (grp) {
		Cluster_t* cluster;
		if (CLUSTER_TARGET_USE_HASH_RING == mode) {
			cluster = (Cluster_t*)consistenthashSelect(&grp->consistent_hash, key);
		}
		else if (CLUSTER_TARGET_USE_HASH_MOD == mode) {
			ListNode_t* cur;
			unsigned int i;
			key %= grp->clusterlistcnt;
			for (i = 0, cur = grp->clusterlist.head; cur && i < key; cur = cur->next, ++i);
			if (!cur)
				return NULL;
			cluster = pod_container_of(cur, Cluster_t, m_grp_listnode);
		}
		else {
			return NULL;
		}
		return cluster;
	}
	return NULL;
}

Channel_t* clusterChannel(Cluster_t* cluster) {
	Channel_t* channel;
	if (cluster == g_ClusterSelf)
		return NULL;
	channel = sessionChannel(&cluster->session);
	if (!channel) {
		Sockaddr_t saddr;
		ReactorObject_t* o;
		int family = ipstrFamily(cluster->ip);
		if (!sockaddrEncode(&saddr.st, family, cluster->ip, cluster->port)) {
			return NULL;
		}
		o = reactorobjectOpen(INVALID_FD_HANDLE, family, cluster->socktype, 0);
		if (!o)
			return NULL;
		channel = openChannel(o, CHANNEL_FLAG_CLIENT, &saddr);
		if (!channel) {
			reactorCommitCmd(NULL, &o->freecmd);
			return NULL;
		}
		channel->_.on_syn_ack = defaultOnSynAck;
		channel->on_heartbeat = defaultOnHeartbeat;
		sessionChannelReplaceClient(&cluster->session, channel);
		reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);
	}
	return channel;
}

#ifdef __cplusplus
}
#endif