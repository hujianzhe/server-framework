#include "config.h"
#include "global.h"
#include "cluster.h"
#include <string.h>

typedef struct ClusterTable_t {
	List_t cluster_list;
	Hashtable_t grp_table;
	HashtableNode_t* grp_bulk[32];
} ClusterTable_t;

Cluster_t* g_ClusterSelf;
struct ClusterTable_t* g_ClusterTable;
int g_ClusterTableVersion;

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
	grp->target_loopcnt = 0;
	return grp;
}

static int cluster_reg_consistenthash(ClusterGroup_t* grp, Cluster_t* cluster) {
	int i;
	for (i = 0; i < cluster->hashkey_cnt; ++i) {
		if (!consistenthashReg(&grp->consistent_hash, cluster->hashkey[i], cluster))
			break;
	}
	if (i != cluster->hashkey_cnt) {
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

#ifdef __cplusplus
extern "C" {
#endif

Cluster_t* getClusterSelf(void) { return g_ClusterSelf; }
void setClusterSelf(Cluster_t* cluster) { g_ClusterSelf = cluster; }
struct ClusterTable_t* ptr_g_ClusterTable(void) { return g_ClusterTable; }
void set_g_ClusterTable(struct ClusterTable_t* t) { g_ClusterTable = t; }
int getClusterTableVersion(void) { return g_ClusterTableVersion; }
void setClusterTableVersion(int version) { g_ClusterTableVersion = version; }

/**************************************************/

Cluster_t* newCluster(int socktype, IPString_t ip, unsigned short port) {
	Cluster_t* cluster = (Cluster_t*)malloc(sizeof(Cluster_t));
	if (cluster) {
		initSession(&cluster->session);
		cluster->session.persist = 1;
		cluster->grp = NULL;
		cluster->name = "";
		cluster->socktype = socktype;
		if (ip)
			strcpy(cluster->ip, ip);
		else
			cluster->ip[0] = 0;
		cluster->port = port;
		cluster->hashkey = NULL;
		cluster->hashkey_cnt = 0;
		cluster->weight_num = 0;
		cluster->connection_num = 0;
	}
	return cluster;
}

void freeCluster(Cluster_t* cluster) {
	if (cluster) {
		free(cluster->hashkey);
		free(cluster);
		if (cluster == g_ClusterSelf)
			g_ClusterSelf = NULL;
	}
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
		channel = openChannelInner(o, CHANNEL_FLAG_CLIENT, &saddr);
		if (!channel) {
			reactorCommitCmd(NULL, &o->freecmd);
			return NULL;
		}
		sessionChannelReplaceClient(&cluster->session, channel);
		reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);
	}
	return channel;
}

unsigned int* reallocClusterHashKey(Cluster_t* cluster, unsigned int hashkey_cnt) {
	unsigned int* hashkey = (unsigned int*)realloc(cluster->hashkey, sizeof(cluster->hashkey[0]) * hashkey_cnt);
	if (hashkey) {
		if (!cluster->hashkey) {
			unsigned int i;
			for (i = cluster->hashkey_cnt; i < hashkey_cnt; ++i) {
				hashkey[i] = 0;
			}
		}
		cluster->hashkey = hashkey;
		cluster->hashkey_cnt = hashkey_cnt;
	}
	return hashkey;
}

/**************************************************/

struct ClusterTable_t* newClusterTable(void) {
	ClusterTable_t* t = (ClusterTable_t*)malloc(sizeof(ClusterTable_t));
	if (t) {
		hashtableInit(&t->grp_table, t->grp_bulk, sizeof(t->grp_bulk) / sizeof(t->grp_bulk[0]), __keycmp, __keyhash);
		listInit(&t->cluster_list);
	}
	return t;
}

ClusterGroup_t* getClusterGroup(struct ClusterTable_t* t, const char* name) {
	HashtableNode_t* htnode = hashtableSearchKey(&t->grp_table, name);
	return htnode ? pod_container_of(htnode, ClusterGroup_t, m_htnode) : NULL;
}

Cluster_t* getCluster(struct ClusterTable_t* t, const char* name, const IPString_t ip, unsigned short port) {
	ClusterGroup_t* grp = getClusterGroup(t, name);
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

List_t* getClusterList(struct ClusterTable_t* t) { return &t->cluster_list; }

int regCluster(struct ClusterTable_t* t, const char* name, Cluster_t* cluster) {
	ClusterGroup_t* grp;
	if (cluster->session.has_reg) {
		return 1;
	}
	grp = getClusterGroup(t, name);
	if (grp) {
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
		hashtableInsertNode(&t->grp_table, &grp->m_htnode);
	}
	cluster->name = (const char*)grp->m_htnode.key;
	cluster->grp = grp;
	grp->clusterlistcnt++;
	listPushNodeBack(&grp->clusterlist, &cluster->m_grp_listnode);

	listPushNodeBack(&t->cluster_list, &cluster->m_listnode);
	cluster->session.has_reg = 1;
	return 1;
}

void unregCluster(struct ClusterTable_t* t, Cluster_t* cluster) {
	if (cluster->session.has_reg && cluster->grp) {
		ClusterGroup_t* grp = cluster->grp;
		listRemoveNode(&grp->clusterlist, &cluster->m_grp_listnode);
		grp->clusterlistcnt--;
		if (grp->clusterlist.head) {
			consistenthashDelValue(&grp->consistent_hash, cluster);
		}
		else {
			hashtableRemoveNode(&t->grp_table, &grp->m_htnode);
			free_cluster_group(grp);
		}
		listRemoveNode(&t->cluster_list, &cluster->m_listnode);
		cluster->session.has_reg = 0;
		cluster->grp = NULL;
	}
}

void freeClusterTable(struct ClusterTable_t* t) {
	HashtableNode_t* curhtnode, *nexthtnode;
	if (!t)
		return;
	for (curhtnode = hashtableFirstNode(&t->grp_table); curhtnode; curhtnode = nexthtnode) {
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
	free(t);
	if (t == g_ClusterTable)
		g_ClusterTable = NULL;
}

Cluster_t* targetCluster(ClusterGroup_t* grp, int mode, unsigned int key) {
	Cluster_t* dst_cluster;
	if (!grp)
		return NULL;
	if (CLUSTER_TARGET_USE_HASH_RING == mode) {
		dst_cluster = (Cluster_t*)consistenthashSelect(&grp->consistent_hash, key);
	}
	else if (CLUSTER_TARGET_USE_HASH_MOD == mode) {
		ListNode_t* cur;
		unsigned int i;
		key %= grp->clusterlistcnt;
		for (i = 0, cur = grp->clusterlist.head; cur && i < key; cur = cur->next, ++i);
		if (!cur)
			return NULL;
		dst_cluster = pod_container_of(cur, Cluster_t, m_grp_listnode);
	}
	else if (CLUSTER_TARGET_USE_ROUND_ROBIN == mode) {
		ListNode_t* cur;
		unsigned int i;
		if (++grp->target_loopcnt >= grp->clusterlistcnt) {
			grp->target_loopcnt = 0;
		}
		for (i = 0, cur = grp->clusterlist.head; cur && i < grp->target_loopcnt; cur = cur->next, ++i);
		if (!cur)
			return NULL;
		dst_cluster = pod_container_of(cur, Cluster_t, m_grp_listnode);
	}
	else if (CLUSTER_TARGET_USE_WEIGHT_NUM == mode) {
		static int mt_seedval = 1;
		int random_val;
		RandMT19937_t mt_ctx;
		int weight_num = 0;
		ListNode_t* cur;
		for (cur = grp->clusterlist.head; cur; cur = cur->next) {
			Cluster_t* exist_cluster = pod_container_of(cur, Cluster_t, m_grp_listnode);
			if (exist_cluster->weight_num <= 0)
				continue;
			weight_num += exist_cluster->weight_num;
		}
		if (0 == weight_num)
			return NULL;
		mt19937Seed(&mt_ctx, mt_seedval++);
		random_val = mt19937Range(&mt_ctx, 0, weight_num);
		weight_num = 0;
		dst_cluster = NULL;
		for (cur = grp->clusterlist.head; cur; cur = cur->next) {
			Cluster_t* exist_cluster = pod_container_of(cur, Cluster_t, m_grp_listnode);
			if (exist_cluster->weight_num <= 0)
				continue;
			weight_num += exist_cluster->weight_num;
			if (random_val < weight_num) {
				dst_cluster = exist_cluster;
				break;
			}
		}
	}
	else if (CLUSTER_TARGET_USE_CONNECT_NUM == mode) {
		ListNode_t* cur;
		dst_cluster = NULL;
		for (cur = grp->clusterlist.head; cur; cur = cur->next) {
			Cluster_t* exist_cluster = pod_container_of(cur, Cluster_t, m_grp_listnode);
			if (!dst_cluster || dst_cluster->connection_num > exist_cluster->connection_num)
				dst_cluster = exist_cluster;
		}
	}
	else {
		return NULL;
	}
	return dst_cluster;
}

#ifdef __cplusplus
}
#endif