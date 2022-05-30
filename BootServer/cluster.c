#include "global.h"
#include "cluster.h"
#include "task_thread.h"

typedef struct ClusterTable_t {
	List_t nodelist;
	Hashtable_t grp_table;
	HashtableNode_t* grp_bulk[32];
	Hashtable_t id_table;
	HashtableNode_t* id_bulk[32];
} ClusterTable_t;

static struct ClusterNodeGroup_t* get_cluster_node_group(struct ClusterTable_t* t, const char* grp_name) {
	HashtableNodeKey_t hkey;
	HashtableNode_t* htnode;
	hkey.ptr = grp_name;
	htnode = hashtableSearchKey(&t->grp_table, hkey);
	return htnode ? pod_container_of(htnode, ClusterNodeGroup_t, m_htnode) : NULL;
}

static void add_cluster_node(struct ClusterTable_t* t, ClusterNode_t* clsnd) {
	clsnd->status = CLSND_STATUS_NORMAL;
	if (clsnd->session.has_reg) {
		return;
	}
	hashtableInsertNode(&t->id_table, &clsnd->m_id_htnode);
	listPushNodeBack(&t->nodelist, &clsnd->m_listnode);
	clsnd->session.has_reg = 1;
}

static void add_cluster_group(struct ClusterTable_t* t, struct ClusterNodeGroup_t* grp) {
	size_t j;
	HashtableNode_t* exist_node = hashtableInsertNode(&t->grp_table, &grp->m_htnode);
	if (exist_node != &grp->m_htnode) {
		struct ClusterNodeGroup_t* exist_grp = pod_container_of(exist_node, ClusterNodeGroup_t, m_htnode);
		hashtableReplaceNode(&t->grp_table, exist_node, &grp->m_htnode);
		freeClusterNodeGroup(exist_grp);
	}
	for (j = 0; j < grp->clsnds.len; ++j) {
		ClusterNode_t* clsnd = grp->clsnds.buf[j];
		add_cluster_node(t, clsnd);
	}
}

#ifdef __cplusplus
extern "C" {
#endif

struct ClusterNodeGroup_t* newClusterNodeGroup(const char* name) {
	ClusterNodeGroup_t* grp = (ClusterNodeGroup_t*)malloc(sizeof(ClusterNodeGroup_t));
	if (!grp) {
		return NULL;
	}
	grp->name = strdup(name);
	if (!grp->name) {
		free(grp);
		return NULL;
	}
	grp->m_htnode.key.ptr = grp->name;
	rbtreeInit(&grp->weight_num_ring, rbtreeDefaultKeyCmpU32);
	rbtreeInit(&grp->consistent_hash_ring, rbtreeDefaultKeyCmpU32);
	dynarrInitZero(&grp->clsnds);
	grp->target_loopcnt = 0;
	grp->total_weight = 0;
	return grp;
}

int regClusterNodeToGroup(struct ClusterNodeGroup_t* grp, ClusterNode_t* clsnd) {
	int ret_ok;
	dynarrInsert(&grp->clsnds, grp->clsnds.len, clsnd, ret_ok);
	return ret_ok;
}

void freeClusterNodeGroup(struct ClusterNodeGroup_t* grp) {
	RBTreeNode_t* tcur, * tnext;
	for (tcur = rbtreeFirstNode(&grp->weight_num_ring); tcur; tcur = tnext) {
		tnext = rbtreeNextNode(tcur);
		rbtreeRemoveNode(&grp->weight_num_ring, tcur);
		free(tcur);
	}
	for (tcur = rbtreeFirstNode(&grp->consistent_hash_ring); tcur; tcur = tnext) {
		tnext = rbtreeNextNode(tcur);
		rbtreeRemoveNode(&grp->consistent_hash_ring, tcur);
		free(tcur);
	}
	dynarrFreeMemory(&grp->clsnds);
	free((void*)grp->name);
	free(grp);
}

void replaceClusterNodeGroup(struct ClusterTable_t* t, struct ClusterNodeGroup_t** grps, size_t grp_cnt) {
	size_t i;
	ListNode_t* lcur;
	HashtableNode_t* curhtnode, * nexthtnode;
	for (curhtnode = hashtableFirstNode(&t->grp_table); curhtnode; curhtnode = nexthtnode) {
		ClusterNodeGroup_t* grp = pod_container_of(curhtnode, ClusterNodeGroup_t, m_htnode);
		nexthtnode = hashtableNextNode(curhtnode);
		freeClusterNodeGroup(grp);
	}
	hashtableInit(&t->grp_table, t->grp_bulk, sizeof(t->grp_bulk) / sizeof(t->grp_bulk[0]), hashtableDefaultKeyCmpStr, hashtableDefaultKeyHashStr);
	for (lcur = t->nodelist.head; lcur; lcur = lcur->next) {
		ClusterNode_t* clsnd = pod_container_of(lcur, ClusterNode_t, m_listnode);
		clsnd->status = CLSND_STATUS_INACTIVE;
	}

	for (i = 0; i < grp_cnt; ++i) {
		add_cluster_group(t, grps[i]);
	}
}

struct ClusterTable_t* newClusterTable(void) {
	ClusterTable_t* t = (ClusterTable_t*)malloc(sizeof(ClusterTable_t));
	if (t) {
		hashtableInit(&t->grp_table, t->grp_bulk, sizeof(t->grp_bulk) / sizeof(t->grp_bulk[0]), hashtableDefaultKeyCmpStr, hashtableDefaultKeyHashStr);
		hashtableInit(&t->id_table, t->id_bulk, sizeof(t->id_bulk) / sizeof(t->id_bulk[0]), hashtableDefaultKeyCmp32, hashtableDefaultKeyHash32);
		listInit(&t->nodelist);
	}
	return t;
}

ClusterNode_t* getClusterNodeById(struct ClusterTable_t* t, int clsnd_id) {
	HashtableNodeKey_t hkey;
	HashtableNode_t* htnode;
	hkey.i32 = clsnd_id;
	htnode = hashtableSearchKey(&t->id_table, hkey);
	return htnode ? pod_container_of(htnode, ClusterNode_t, m_id_htnode) : NULL;
}

void getClusterNodes(struct ClusterTable_t* t, DynArrClusterNodePtr_t* v) {
	ListNode_t* cur;
	for (cur = t->nodelist.head; cur; cur = cur->next) {
		ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_listnode);
		int ret_ok;
		dynarrInsert(v, v->len, clsnd, ret_ok);
		if (!ret_ok) {
			break;
		}
	}
}

void getClusterGroupNodes(struct ClusterTable_t* t, const char* grp_name, DynArrClusterNodePtr_t* v) {
	int ret_ok;
	ClusterNodeGroup_t* grp = get_cluster_node_group(t, grp_name);
	if (!grp) {
		return;
	}
	dynarrCopyAppend(v, grp->clsnds.buf, grp->clsnds.len, ret_ok);
}

void freeClusterTable(struct ClusterTable_t* t) {
	if (t) {
		ListNode_t* curlnode, * nextlnode;
		HashtableNode_t* curhtnode, * nexthtnode;
		for (curhtnode = hashtableFirstNode(&t->grp_table); curhtnode; curhtnode = nexthtnode) {
			ClusterNodeGroup_t* grp = pod_container_of(curhtnode, ClusterNodeGroup_t, m_htnode);
			nexthtnode = hashtableNextNode(curhtnode);
			freeClusterNodeGroup(grp);
		}
		for (curlnode = t->nodelist.head; curlnode; curlnode = nextlnode) {
			ClusterNode_t* clsnd = pod_container_of(curlnode, ClusterNode_t, m_listnode);
			nextlnode = curlnode->next;
			free(clsnd);
		}
		free(t);
	}
}

ClusterNode_t* targetClusterNode(struct ClusterTable_t* t, const char* grp_name, int mode, unsigned int key) {
	ClusterNode_t* dst_clsnd;
	ClusterNodeGroup_t* grp = get_cluster_node_group(t, grp_name);
	if (!grp) {
		return NULL;
	}
	if (CLUSTER_TARGET_USE_HASH_RING == mode) {
		RBTreeNodeKey_t rkey;
		RBTreeNode_t* exist_node;
		struct {
			RBTreeNode_t _;
			ClusterNode_t* clsnd;
		} *data;
		rkey.u32 = key;
		exist_node = rbtreeUpperBoundKey(&grp->consistent_hash_ring, rkey);
		if (!exist_node) {
			exist_node = rbtreeFirstNode(&grp->consistent_hash_ring);
			if (!exist_node) {
				return NULL;
			}
		}
		*(void**)&data = exist_node;
		dst_clsnd = data->clsnd;
	}
	else if (CLUSTER_TARGET_USE_HASH_MOD == mode) {
		if (grp->clsnds.len <= 0) {
			return NULL;
		}
		key %= grp->clsnds.len;
		dst_clsnd = grp->clsnds.buf[key];
	}
	else if (CLUSTER_TARGET_USE_ROUND_ROBIN == mode) {
		if (grp->clsnds.len <= 0) {
			return NULL;
		}
		if (++grp->target_loopcnt >= grp->clsnds.len) {
			grp->target_loopcnt = 0;
		}
		dst_clsnd = grp->clsnds.buf[grp->target_loopcnt];
	}
	else if (CLUSTER_TARGET_USE_RANDOM == mode) {
		static int mt_seedval = 1;
		int random_val;
		RandMT19937_t mt_ctx;
		if (grp->clsnds.len <= 0) {
			return NULL;
		}
		mt19937Seed(&mt_ctx, mt_seedval++);
		random_val = mt19937Range(&mt_ctx, 0, grp->clsnds.len);
		dst_clsnd = grp->clsnds.buf[random_val];
	}
	else if (CLUSTER_TARGET_USE_WEIGHT_RANDOM == mode) {
		static int mt_seedval = 1;
		RandMT19937_t mt_ctx;
		RBTreeNodeKey_t rkey;
		RBTreeNode_t* exist_node;
		struct {
			RBTreeNode_t _;
			ClusterNode_t* clsnd;
		} *data;
		if (grp->total_weight <= 0) {
			return NULL;
		}
		mt19937Seed(&mt_ctx, mt_seedval++);
		rkey.u32 = mt19937Range(&mt_ctx, 0, grp->total_weight);
		exist_node = rbtreeUpperBoundKey(&grp->weight_num_ring, rkey);
		if (!exist_node) {
			return NULL;
		}
		*(void**)&data = exist_node;
		dst_clsnd = data->clsnd;
	}
	else {
		return NULL;
	}
	return dst_clsnd;
}

void broadcastClusterGroup(struct ClusterTable_t* t, const char* grp_name, const Iobuf_t iov[], unsigned int iovcnt) {
	ClusterNodeGroup_t* grp = get_cluster_node_group(t, grp_name);
	if (grp) {
		int self_id = ptrBSG()->conf->clsnd.id;
		size_t i;
		for (i = 0; i < grp->clsnds.len; ++i) {
			Channel_t* channel;
			ClusterNode_t* clsnd = grp->clsnds.buf[i];
			if (clsnd->id == self_id) {
				continue;
			}
			channel = connectClusterNode(clsnd);
			if (!channel) {
				continue;
			}
			channelSendv(channel, iov, iovcnt, NETPACKET_FRAGMENT);
		}
	}
}

void broadcastClusterTable(struct ClusterTable_t* t, const Iobuf_t iov[], unsigned int iovcnt) {
	ListNode_t* cur;
	for (cur = t->nodelist.head; cur; cur = cur->next) {
		Channel_t* channel;
		ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_listnode);
		int self_id = ptrBSG()->conf->clsnd.id;
		if (clsnd->id == self_id) {
			continue;
		}
		channel = connectClusterNode(clsnd);
		if (!channel) {
			continue;
		}
		channelSendv(channel, iov, iovcnt, NETPACKET_FRAGMENT);
	}
}

#ifdef __cplusplus
}
#endif
