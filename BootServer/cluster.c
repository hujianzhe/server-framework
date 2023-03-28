#include "global.h"
#include "cluster.h"
#include "task_thread.h"
#include <string.h>
#include <stdlib.h>

typedef struct ClusterTable_t {
	List_t nodelist;
	Hashtable_t grp_table;
	HashtableNode_t* grp_bulk[32];
	Hashtable_t ident_table;
	HashtableNode_t* ident_bulk[32];
} ClusterTable_t;

typedef struct PairNumClsnd_t {
	unsigned int num;
	ClusterNode_t* clsnd;
} PairNumClsnd_t;

typedef struct ClusterNodeGroup_t {
	HashtableNode_t m_htnode;
	const char* name;
	DynArr_t(PairNumClsnd_t) weight_clsnds;
	DynArr_t(PairNumClsnd_t) consistent_hash_clsnds;
	DynArrClusterNodePtr_t clsnds;
	unsigned int target_loopcnt;
	unsigned int total_weight;
	int consistent_hash_sorted;
} ClusterNodeGroup_t;

const char* ClusterNodeGroup_name(struct ClusterNodeGroup_t* grp) { return grp->name; }

static void add_cluster_node(struct ClusterTable_t* t, ClusterNode_t* clsnd) {
	clsnd->status = CLSND_STATUS_NORMAL;
	if (clsnd->session.has_reg) {
		return;
	}
	hashtableInsertNode(&t->ident_table, &clsnd->m_ident_htnode);
	listPushNodeBack(&t->nodelist, &clsnd->m_listnode);
	clsnd->session.has_reg = 1;
}

static int fn_consistent_hash_compare(const void* p1, const void* p2) {
	return ((const PairNumClsnd_t*)p1)->num > ((const PairNumClsnd_t*)p2)->num;
}

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************/

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
	dynarrInitZero(&grp->weight_clsnds);
	dynarrInitZero(&grp->consistent_hash_clsnds);
	dynarrInitZero(&grp->clsnds);
	grp->target_loopcnt = 0;
	grp->total_weight = 0;
	grp->consistent_hash_sorted = 0;
	return grp;
}

int regClusterNodeToGroup(struct ClusterNodeGroup_t* grp, ClusterNode_t* clsnd) {
	int ret_ok;
	dynarrInsert(&grp->clsnds, grp->clsnds.len, clsnd, ret_ok);
	return ret_ok;
}

int regClusterNodeToGroupByHashKey(struct ClusterNodeGroup_t* grp, unsigned int hashkey, ClusterNode_t* clsnd) {
	int ret_ok;
	PairNumClsnd_t data = { hashkey, clsnd };
	dynarrInsert(&grp->consistent_hash_clsnds, grp->consistent_hash_clsnds.len, data, ret_ok);
	if (ret_ok) {
		grp->consistent_hash_sorted = 0;
	}
	return ret_ok;
}

int regClusterNodeToGroupByWeight(struct ClusterNodeGroup_t* grp, int weight, ClusterNode_t* clsnd) {
	size_t i, exist = 0;
	unsigned int total_weight = 0;
	for (i = 0; i < grp->weight_clsnds.len; ++i) {
		PairNumClsnd_t* data = grp->weight_clsnds.buf + i;
		if (data->clsnd == clsnd) {
			if (data->num == weight) {
				return 1;
			}
			data->num = weight;
			exist = 1;
		}
		total_weight += data->num;
	}
	if (!exist) {
		int ret_ok;
		PairNumClsnd_t data = { weight, clsnd };
		dynarrInsert(&grp->weight_clsnds, grp->weight_clsnds.len, data, ret_ok);
		if (!ret_ok) {
			return 0;
		}
		total_weight += weight;
	}
	grp->total_weight = total_weight;
	return 1;
}

void delCluserNodeFromGroup(struct ClusterNodeGroup_t* grp, const char* clsnd_ident) {
	size_t i;
	for (i = 0; i < grp->clsnds.len; ++i) {
		if (0 == strcmp(grp->clsnds.buf[i]->ident, clsnd_ident)) {
			dynarrSwapRemoveIdx(&grp->clsnds, i);
			break;
		}
	}
	for (i = 0; i < grp->weight_clsnds.len; ++i) {
		PairNumClsnd_t* data = grp->weight_clsnds.buf + i;
		if (0 == strcmp(data->clsnd->ident, clsnd_ident)) {
			dynarrSwapRemoveIdx(&grp->weight_clsnds, i);
			break;
		}
	}
	for (i = 0; i < grp->consistent_hash_clsnds.len; ) {
		PairNumClsnd_t* data = grp->consistent_hash_clsnds.buf + i;
		if (0 == strcmp(data->clsnd->ident, clsnd_ident)) {
			dynarrSwapRemoveIdx(&grp->consistent_hash_clsnds, i);
			continue;
		}
		++i;
	}
}

void freeClusterNodeGroup(struct ClusterNodeGroup_t* grp) {
	dynarrFreeMemory(&grp->weight_clsnds);
	dynarrFreeMemory(&grp->consistent_hash_clsnds);
	dynarrFreeMemory(&grp->clsnds);
	free((void*)grp->name);
	free(grp);
}

/****************************************************************/

struct ClusterTable_t* newClusterTable(void) {
	ClusterTable_t* t = (ClusterTable_t*)malloc(sizeof(ClusterTable_t));
	if (t) {
		hashtableInit(&t->grp_table, t->grp_bulk, sizeof(t->grp_bulk) / sizeof(t->grp_bulk[0]), hashtableDefaultKeyCmpStr, hashtableDefaultKeyHashStr);
		hashtableInit(&t->ident_table, t->ident_bulk, sizeof(t->ident_bulk) / sizeof(t->ident_bulk[0]), hashtableDefaultKeyCmpStr, hashtableDefaultKeyHashStr);
		listInit(&t->nodelist);
	}
	return t;
}

struct ClusterNodeGroup_t* getClusterNodeGroup(struct ClusterTable_t* t, const char* grp_name) {
	/* node: async proc maybe dangerous, cluster node group maybe already free */
	HashtableNodeKey_t hkey;
	HashtableNode_t* htnode;
	hkey.ptr = grp_name;
	htnode = hashtableSearchKey(&t->grp_table, hkey);
	return htnode ? pod_container_of(htnode, ClusterNodeGroup_t, m_htnode) : NULL;
}

void clearClusterNodeGroup(struct ClusterTable_t* t) {
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
}

void replaceClusterNodeGroup(struct ClusterTable_t* t, struct ClusterNodeGroup_t* grp) {
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

ClusterNode_t* getClusterNodeById(struct ClusterTable_t* t, const char* clsnd_ident) {
	HashtableNodeKey_t hkey;
	HashtableNode_t* htnode;
	hkey.ptr = clsnd_ident;
	htnode = hashtableSearchKey(&t->ident_table, hkey);
	return htnode ? pod_container_of(htnode, ClusterNode_t, m_ident_htnode) : NULL;
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
	ClusterNodeGroup_t* grp = getClusterNodeGroup(t, grp_name);
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
			freeClusterNode(clsnd);
		}
		free(t);
	}
}

void inactiveClusterNode(struct ClusterTable_t* t, ClusterNode_t* clsnd) {
	HashtableNode_t* curhtnode, * nexthtnode;
	for (curhtnode = hashtableFirstNode(&t->grp_table); curhtnode; curhtnode = nexthtnode) {
		ClusterNodeGroup_t* grp = pod_container_of(curhtnode, ClusterNodeGroup_t, m_htnode);
		nexthtnode = hashtableNextNode(curhtnode);
		delCluserNodeFromGroup(grp, clsnd->ident);
	}
	clsnd->status = CLSND_STATUS_INACTIVE;
}

ClusterNode_t* targetClusterNode(struct ClusterTable_t* t, const char* grp_name, int mode, unsigned int key) {
	ClusterNode_t* dst_clsnd;
	ClusterNodeGroup_t* grp = getClusterNodeGroup(t, grp_name);
	if (!grp) {
		return NULL;
	}
	if (CLUSTER_TARGET_USE_HASH_RING == mode) {
		size_t i;
		if (!grp->consistent_hash_sorted) {
			grp->consistent_hash_sorted = 1;
			qsort(grp->consistent_hash_clsnds.buf, grp->consistent_hash_clsnds.len, sizeof(grp->consistent_hash_clsnds.buf[0]), fn_consistent_hash_compare);
		}
		for (i = 0; i < grp->consistent_hash_clsnds.len; ++i) {
			PairNumClsnd_t* data = grp->consistent_hash_clsnds.buf + i;
			if (data->num > key) {
				dst_clsnd = data->clsnd;
				break;
			}
		}
		if (!dst_clsnd && grp->consistent_hash_clsnds.len > 0) {
			dst_clsnd = grp->consistent_hash_clsnds.buf[0].clsnd;
		}
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
		size_t i;
		unsigned int weight;
		if (grp->total_weight <= 0) {
			return NULL;
		}
		mt19937Seed(&mt_ctx, mt_seedval++);
		weight = mt19937Range(&mt_ctx, 0, grp->total_weight);
		for (i = 0; i < grp->weight_clsnds.len; ++i) {
			PairNumClsnd_t* data = grp->weight_clsnds.buf + i;
			if (weight <= data->num) {
				dst_clsnd = data->clsnd;
				break;
			}
			weight -= data->num;
		}
	}
	else if (CLUSTER_TARGET_USE_FACTOR_MIN == mode) {
		size_t i;
		dst_clsnd = NULL;
		for (i = 0; i < grp->clsnds.len; ++i) {
			ClusterNode_t* clsnd = grp->clsnds.buf[i];
			if (!dst_clsnd || dst_clsnd->factor > clsnd->factor) {
				dst_clsnd = clsnd;
			}
		}
	}
	else if (CLUSTER_TARGET_USE_FACTOR_MAX == mode) {
		size_t i;
		dst_clsnd = NULL;
		for (i = 0; i < grp->clsnds.len; ++i) {
			ClusterNode_t* clsnd = grp->clsnds.buf[i];
			if (!dst_clsnd || dst_clsnd->factor < clsnd->factor) {
				dst_clsnd = clsnd;
			}
		}
	}
	else {
		return NULL;
	}
	return dst_clsnd;
}

void broadcastClusterGroup(struct ClusterTable_t* t, const char* grp_name, const Iobuf_t iov[], unsigned int iovcnt) {
	ClusterNodeGroup_t* grp = getClusterNodeGroup(t, grp_name);
	if (grp) {
		size_t i;
		for (i = 0; i < grp->clsnds.len; ++i) {
			ClusterNode_t* clsnd = grp->clsnds.buf[i];
			clsndSendv(clsnd, iov, iovcnt);
		}
	}
}

void broadcastClusterTable(struct ClusterTable_t* t, const Iobuf_t iov[], unsigned int iovcnt) {
	ListNode_t* cur;
	for (cur = t->nodelist.head; cur; cur = cur->next) {
		ClusterNode_t* clsnd = pod_container_of(cur, ClusterNode_t, m_listnode);
		clsndSendv(clsnd, iov, iovcnt);
	}
}

#ifdef __cplusplus
}
#endif
