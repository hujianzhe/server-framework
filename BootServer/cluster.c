#include "global.h"
#include "cluster.h"

typedef struct ClusterTable_t {
	List_t nodelist;
	Hashtable_t grp_table;
	HashtableNode_t* grp_bulk[32];
	Hashtable_t id_table;
	HashtableNode_t* id_bulk[32];
} ClusterTable_t;

typedef struct ClusterNodeGroup_t {
	HashtableNode_t m_htnode;
	List_t nodelist;
	unsigned int nodelistcnt;
	ConsistentHash_t consistent_hash;
	unsigned int target_loopcnt;
} ClusterNodeGroup_t;

static int __grp_name_keycmp(const void* node_key, const void* key) { return strcmp((const char*)node_key, (const char*)key); }
static unsigned int __grp_name_keyhash(const void* key) { return hashBKDR((const char*)key); }

static ClusterNodeGroup_t* new_cluster_node_group(const char* name) {
	ClusterNodeGroup_t* grp = (ClusterNodeGroup_t*)malloc(sizeof(ClusterNodeGroup_t));
	if (!grp)
		return NULL;
	grp->m_htnode.key = strdup(name);
	if (!grp->m_htnode.key) {
		free(grp);
		return NULL;
	}
	grp->nodelistcnt = 0;
	listInit(&grp->nodelist);
	consistenthashInit(&grp->consistent_hash);
	grp->target_loopcnt = 0;
	return grp;
}

static int cluster_reg_consistenthash(ClusterNodeGroup_t* grp, ClusterNode_t* clsnd) {
	int i;
	for (i = 0; i < clsnd->hashkey_cnt; ++i) {
		if (!consistenthashReg(&grp->consistent_hash, clsnd->hashkey[i], clsnd))
			break;
	}
	if (i != clsnd->hashkey_cnt) {
		consistenthashDelValue(&grp->consistent_hash, clsnd);
		return 0;
	}
	return 1;
}

static void free_cluster_node_group(ClusterNodeGroup_t* grp) {
	free((void*)grp->m_htnode.key);
	consistenthashFree(&grp->consistent_hash);
	free(grp);
}

#ifdef __cplusplus
extern "C" {
#endif

struct ClusterTable_t* newClusterTable(void) {
	ClusterTable_t* t = (ClusterTable_t*)malloc(sizeof(ClusterTable_t));
	if (t) {
		hashtableInit(&t->grp_table, t->grp_bulk, sizeof(t->grp_bulk) / sizeof(t->grp_bulk[0]), __grp_name_keycmp, __grp_name_keyhash);
		hashtableInit(&t->id_table, t->id_bulk, sizeof(t->id_bulk) / sizeof(t->id_bulk[0]), NULL, NULL);
		listInit(&t->nodelist);
	}
	return t;
}

struct ClusterNodeGroup_t* getClusterNodeGroup(struct ClusterTable_t* t, const char* name) {
	HashtableNode_t* htnode = hashtableSearchKey(&t->grp_table, name);
	return htnode ? pod_container_of(htnode, ClusterNodeGroup_t, m_htnode) : NULL;
}

ClusterNode_t* getClusterNodeFromGroup(struct ClusterNodeGroup_t* grp, int socktype, const IPString_t ip, unsigned short port) {
	if (grp) {
		ListNode_t* cur;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (!strcmp(clsnd->ip, ip) && clsnd->port == port && clsnd->socktype == socktype) {
				return clsnd;
			}
		}
	}
	return NULL;
}

ClusterNode_t* getClusterNode(struct ClusterTable_t* t, int socktype, const IPString_t ip, unsigned short port) {
	ListNode_t* cur;
	for (cur = t->nodelist.head; cur; cur = cur->next) {
		ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_listnode);
		if (!strcmp(clsnd->ip, ip) && clsnd->port == port && clsnd->socktype == socktype) {
			return clsnd;
		}
	}
	return NULL;
}

ClusterNode_t* getClusterNodeById(struct ClusterTable_t* t, int clsnd_id) {
	HashtableNode_t* htnode = hashtableSearchKey(&t->id_table, (void*)(size_t)clsnd_id);
	return htnode ? pod_container_of(htnode, ClusterNode_t, m_id_htnode) : NULL;
}

List_t* getClusterNodeList(struct ClusterTable_t* t) { return &t->nodelist; }

int regClusterNode(struct ClusterTable_t* t, const char* name, ClusterNode_t* clsnd) {
	ClusterNodeGroup_t* grp;
	if (clsnd->session.has_reg) {
		return 1;
	}
	grp = getClusterNodeGroup(t, name);
	if (grp) {
		if (!cluster_reg_consistenthash(grp, clsnd)) {
			return 0;
		}
	}
	else {
		grp = new_cluster_node_group(name);
		if (!grp)
			return 0;
		if (!cluster_reg_consistenthash(grp, clsnd)) {
			return 0;
		}
		hashtableInsertNode(&t->grp_table, &grp->m_htnode);
	}
	clsnd->name = (const char*)grp->m_htnode.key;
	clsnd->grp = grp;
	grp->nodelistcnt++;
	listPushNodeBack(&grp->nodelist, &clsnd->m_grp_listnode);
	hashtableInsertNode(&t->id_table, &clsnd->m_id_htnode);
	listPushNodeBack(&t->nodelist, &clsnd->m_listnode);
	clsnd->session.has_reg = 1;
	return 1;
}

void unregClusterNode(struct ClusterTable_t* t, ClusterNode_t* clsnd) {
	if (clsnd->session.has_reg && clsnd->grp) {
		ClusterNodeGroup_t* grp = clsnd->grp;
		listRemoveNode(&grp->nodelist, &clsnd->m_grp_listnode);
		grp->nodelistcnt--;
		if (grp->nodelist.head) {
			consistenthashDelValue(&grp->consistent_hash, clsnd);
		}
		else {
			hashtableRemoveNode(&t->grp_table, &grp->m_htnode);
			free_cluster_node_group(grp);
		}
		hashtableRemoveNode(&t->id_table, &clsnd->m_id_htnode);
		listRemoveNode(&t->nodelist, &clsnd->m_listnode);
		clsnd->session.has_reg = 0;
		clsnd->grp = NULL;
	}
}

void freeClusterTable(struct ClusterTable_t* t) {
	HashtableNode_t* curhtnode, *nexthtnode;
	if (!t)
		return;
	for (curhtnode = hashtableFirstNode(&t->grp_table); curhtnode; curhtnode = nexthtnode) {
		ListNode_t* curlistnode, *nextlistnode;
		ClusterNodeGroup_t* grp = pod_container_of(curhtnode, ClusterNodeGroup_t, m_htnode);
		nexthtnode = hashtableNextNode(curhtnode);
		for (curlistnode = grp->nodelist.head; curlistnode; curlistnode = nextlistnode) {
			ClusterNode_t* clsnd = pod_container_of(curlistnode, ClusterNode_t, m_grp_listnode);
			nextlistnode = curlistnode->next;
			freeClusterNode(clsnd);
		}
		free_cluster_node_group(grp);
	}
	free(t);
}

ClusterNode_t* targetClusterNode(struct ClusterNodeGroup_t* grp, int mode, unsigned int key) {
	ClusterNode_t* dst_clsnd;
	if (!grp)
		return NULL;
	if (CLUSTER_TARGET_USE_HASH_RING == mode) {
		dst_clsnd = (ClusterNode_t*)consistenthashSelect(&grp->consistent_hash, key);
	}
	else if (CLUSTER_TARGET_USE_HASH_MOD == mode) {
		ListNode_t* cur;
		key %= grp->nodelistcnt;
		cur = listAt(&grp->nodelist, key);
		if (!cur)
			return NULL;
		dst_clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
	}
	else if (CLUSTER_TARGET_USE_ROUND_ROBIN == mode) {
		ListNode_t* cur;
		if (++grp->target_loopcnt >= grp->nodelistcnt) {
			grp->target_loopcnt = 0;
		}
		cur = listAt(&grp->nodelist, grp->target_loopcnt);
		if (!cur)
			return NULL;
		dst_clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
	}
	else if (CLUSTER_TARGET_USE_RANDOM == mode) {
		static int mt_seedval = 1;
		int random_val;
		ListNode_t* cur;
		RandMT19937_t mt_ctx;
		mt19937Seed(&mt_ctx, mt_seedval++);
		random_val = mt19937Range(&mt_ctx, 0, grp->nodelistcnt);
		cur = listAt(&grp->nodelist, random_val);
		if (!cur)
			return NULL;
		dst_clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
	}
	else if (CLUSTER_TARGET_USE_WEIGHT_RANDOM == mode) {
		static int mt_seedval = 1;
		int random_val;
		RandMT19937_t mt_ctx;
		int weight_num = 0;
		ListNode_t* cur;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (clsnd->weight_num <= 0)
				continue;
			weight_num += clsnd->weight_num;
		}
		if (0 == weight_num)
			return NULL;
		mt19937Seed(&mt_ctx, mt_seedval++);
		random_val = mt19937Range(&mt_ctx, 0, weight_num);
		weight_num = 0;
		dst_clsnd = NULL;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (clsnd->weight_num <= 0)
				continue;
			weight_num += clsnd->weight_num;
			if (random_val < weight_num) {
				dst_clsnd = clsnd;
				break;
			}
		}
	}
	else if (CLUSTER_TARGET_USE_WEIGHT_MIN == mode) {
		ListNode_t* cur;
		dst_clsnd = NULL;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (!dst_clsnd || dst_clsnd->weight_num > clsnd->weight_num)
				dst_clsnd = clsnd;
		}
	}
	else if (CLUSTER_TARGET_USE_WEIGHT_MAX == mode) {
		ListNode_t* cur;
		dst_clsnd = NULL;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (!dst_clsnd || dst_clsnd->weight_num < clsnd->weight_num)
				dst_clsnd = clsnd;
		}
	}
	else if (CLUSTER_TARGET_USE_CONNECT_NUM == mode) {
		ListNode_t* cur;
		dst_clsnd = NULL;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (!dst_clsnd || dst_clsnd->connection_num > clsnd->connection_num)
				dst_clsnd = clsnd;
		}
	}
	else {
		return NULL;
	}
	return dst_clsnd;
}

ClusterNode_t* targetClusterNodeByIp(struct ClusterNodeGroup_t* grp, const IPString_t ip, int mode, unsigned int key) {
	ClusterNode_t* dst_clsnd;
	if (!grp || !ip || 0 == ip[0])
		return NULL;
	else if (CLUSTER_TARGET_USE_HASH_MOD == mode) {
		ListNode_t* cur;
		unsigned int nodelistcnt = 0;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (0 == strcmp(clsnd->ip, ip))
				++nodelistcnt;
		}
		if (0 == nodelistcnt)
			return NULL;
		key %= nodelistcnt;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (strcmp(clsnd->ip, ip))
				continue;
			if (0 == key)
				break;
			key--;
		}
		if (!cur)
			return NULL;
		dst_clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
	}
	else if (CLUSTER_TARGET_USE_ROUND_ROBIN == mode) {
		ListNode_t* cur;
		if (++grp->target_loopcnt >= grp->nodelistcnt) {
			grp->target_loopcnt = 0;
		}
		key = grp->target_loopcnt;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (strcmp(clsnd->ip, ip))
				continue;
			if (0 == key)
				break;
			key--;
		}
		if (!cur)
			return NULL;
		dst_clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
	}
	else if (CLUSTER_TARGET_USE_RANDOM == mode) {
		static int mt_seedval = 1;
		ListNode_t* cur;
		RandMT19937_t mt_ctx;
		unsigned int nodelistcnt = 0;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (0 == strcmp(clsnd->ip, ip))
				++nodelistcnt;
		}
		if (0 == nodelistcnt)
			return NULL;
		mt19937Seed(&mt_ctx, mt_seedval++);
		key = mt19937Range(&mt_ctx, 0, nodelistcnt);
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (strcmp(clsnd->ip, ip))
				continue;
			if (0 == key)
				break;
			key--;
		}
		if (!cur)
			return NULL;
		dst_clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
	}
	else if (CLUSTER_TARGET_USE_WEIGHT_RANDOM == mode) {
		static int mt_seedval = 1;
		int random_val;
		RandMT19937_t mt_ctx;
		int weight_num = 0;
		ListNode_t* cur;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (clsnd->weight_num <= 0)
				continue;
			if (strcmp(clsnd->ip, ip))
				continue;
			weight_num += clsnd->weight_num;
		}
		if (0 == weight_num)
			return NULL;
		mt19937Seed(&mt_ctx, mt_seedval++);
		random_val = mt19937Range(&mt_ctx, 0, weight_num);
		weight_num = 0;
		dst_clsnd = NULL;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (clsnd->weight_num <= 0)
				continue;
			if (strcmp(clsnd->ip, ip))
				continue;
			weight_num += clsnd->weight_num;
			if (random_val < weight_num) {
				dst_clsnd = clsnd;
				break;
			}
		}
	}
	else if (CLUSTER_TARGET_USE_WEIGHT_MIN == mode) {
		ListNode_t* cur;
		dst_clsnd = NULL;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (strcmp(clsnd->ip, ip))
				continue;
			if (!dst_clsnd || dst_clsnd->weight_num > clsnd->weight_num)
				dst_clsnd = clsnd;
		}
	}
	else if (CLUSTER_TARGET_USE_WEIGHT_MAX == mode) {
		ListNode_t* cur;
		dst_clsnd = NULL;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (strcmp(clsnd->ip, ip))
				continue;
			if (!dst_clsnd || dst_clsnd->weight_num < clsnd->weight_num)
				dst_clsnd = clsnd;
		}
	}
	else if (CLUSTER_TARGET_USE_CONNECT_NUM == mode) {
		ListNode_t* cur;
		dst_clsnd = NULL;
		for (cur = grp->nodelist.head; cur; cur = cur->next) {
			ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
			if (strcmp(clsnd->ip, ip))
				continue;
			if (!dst_clsnd || dst_clsnd->connection_num > clsnd->connection_num)
				dst_clsnd = clsnd;
		}
	}
	else {
		return NULL;
	}
	return dst_clsnd;
}

void broadcastClusterGroup(struct ClusterNodeGroup_t* grp, const Iobuf_t iov[], unsigned int iovcnt) {
	ListNode_t* cur;
	if (!grp)
		return;
	for (cur = grp->nodelist.head; cur; cur = cur->next) {
		Channel_t* channel;
		ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_grp_listnode);
		if (clsnd == g_SelfClusterNode)
			continue;
		channel = connectClusterNode(clsnd);
		if (!channel)
			continue;
		channelSendv(channel, iov, iovcnt, NETPACKET_FRAGMENT);
	}
}

void broadcastClusterTable(struct ClusterTable_t* t, const Iobuf_t iov[], unsigned int iovcnt) {
	ListNode_t* cur;
	for (cur = t->nodelist.head; cur; cur = cur->next) {
		Channel_t* channel;
		ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_listnode);
		if (clsnd == g_SelfClusterNode)
			continue;
		channel = connectClusterNode(clsnd);
		if (!channel)
			continue;
		channelSendv(channel, iov, iovcnt, NETPACKET_FRAGMENT);
	}
}

#ifdef __cplusplus
}
#endif
