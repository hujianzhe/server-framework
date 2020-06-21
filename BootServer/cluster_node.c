#include "config.h"
#include "global.h"
#include "cluster_node.h"
#include <string.h>

ClusterNode_t* g_SelfClusterNode;

#ifdef __cplusplus
extern "C" {
#endif

ClusterNode_t* selfClusterNode(void) { return g_SelfClusterNode; }
void setSelfClusterNode(ClusterNode_t* cluster) { g_SelfClusterNode = cluster; }

ClusterNode_t* newClusterNode(int socktype, IPString_t ip, unsigned short port) {
	ClusterNode_t* clsnd = (ClusterNode_t*)malloc(sizeof(ClusterNode_t));
	if (clsnd) {
		initSession(&clsnd->session);
		clsnd->session.persist = 1;
		clsnd->grp = NULL;
		clsnd->name = "";
		clsnd->socktype = socktype;
		if (ip)
			strcpy(clsnd->ip, ip);
		else
			clsnd->ip[0] = 0;
		clsnd->port = port;
		clsnd->hashkey = NULL;
		clsnd->hashkey_cnt = 0;
		clsnd->weight_num = 0;
		clsnd->connection_num = 0;
	}
	return clsnd;
}

void freeClusterNode(ClusterNode_t* clsnd) {
	if (clsnd) {
		free(clsnd->hashkey);
		free(clsnd);
		if (clsnd == g_SelfClusterNode)
			g_SelfClusterNode = NULL;
	}
}

Channel_t* connectClusterNode(ClusterNode_t* clsnd) {
	Channel_t* channel;
	if (clsnd == g_SelfClusterNode)
		return NULL;
	channel = sessionChannel(&clsnd->session);
	if (!channel) {
		SendMsg_t msg;
		char* hs_data;
		int hs_datalen;
		Sockaddr_t saddr;
		ReactorObject_t* o;

		hs_data = strFormat(&hs_datalen, "{\"ip\":\"%s\",\"port\":%u,\"socktype\":\"%s\",\"connection_num\":%d}",
			g_SelfClusterNode->ip, g_SelfClusterNode->port,
			if_socktype2string(g_SelfClusterNode->socktype),
			g_SelfClusterNode->connection_num);
		if (!hs_data)
			return NULL;

		if (!sockaddrEncode(&saddr.st, ipstrFamily(clsnd->ip), clsnd->ip, clsnd->port)) {
			free(hs_data);
			return NULL;
		}
		o = reactorobjectOpen(INVALID_FD_HANDLE, saddr.sa.sa_family, clsnd->socktype, 0);
		if (!o) {
			free(hs_data);
			return NULL;
		}
		channel = openChannelInner(o, CHANNEL_FLAG_CLIENT, &saddr);
		if (!channel) {
			reactorCommitCmd(NULL, &o->freecmd);
			free(hs_data);
			return NULL;
		}
		sessionChannelReplaceClient(&clsnd->session, channel);
		reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);
		// handshake
		makeSendMsg(&msg, 0, hs_data, hs_datalen)->rpc_status = 'S';
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		free(hs_data);
	}
	return channel;
}

unsigned int* reallocClusterNodeHashKey(ClusterNode_t* clsnd, unsigned int hashkey_cnt) {
	unsigned int* hashkey = (unsigned int*)realloc(clsnd->hashkey, sizeof(clsnd->hashkey[0]) * hashkey_cnt);
	if (hashkey) {
		if (!clsnd->hashkey) {
			unsigned int i;
			for (i = clsnd->hashkey_cnt; i < hashkey_cnt; ++i) {
				hashkey[i] = 0;
			}
		}
		clsnd->hashkey = hashkey;
		clsnd->hashkey_cnt = hashkey_cnt;
	}
	return hashkey;
}

#ifdef __cplusplus
}
#endif