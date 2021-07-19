#include "config.h"
#include "global.h"
#include "cluster_node.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

ClusterNode_t* newClusterNode(int id, int socktype, IPString_t ip, unsigned short port) {
	ClusterNode_t* clsnd = (ClusterNode_t*)malloc(sizeof(ClusterNode_t));
	if (clsnd) {
		initSession(&clsnd->session);
		clsnd->m_id_htnode.key = (void*)(size_t)id;
		clsnd->name = "";
		clsnd->id = id;
		clsnd->socktype = socktype;
		if (ip) {
			strcpy(clsnd->ip, ip);
		}
		else {
			clsnd->ip[0] = 0;
		}
		clsnd->port = port;
		clsnd->connection_num = 0;
		clsnd->status = CLSND_STATUS_NORMAL;
	}
	return clsnd;
}

void freeClusterNode(ClusterNode_t* clsnd) {
	free(clsnd);
}

Channel_t* connectClusterNode(ClusterNode_t* clsnd, struct DataQueue_t* dq) {
	Channel_t* channel;
	if (clsnd->id == g_Config.clsnd.id) {
		return NULL;
	}
	channel = sessionChannel(&clsnd->session);
	if (!channel) {
		InnerMsg_t msg;
		char* hs_data;
		int hs_datalen;
		Sockaddr_t saddr;
		ReactorObject_t* o;

		if (clsnd->session.reconnect_timestamp_sec > 0 &&
			time(NULL) < clsnd->session.reconnect_timestamp_sec)
		{
			return NULL;
		}

		hs_data = strFormat(&hs_datalen, "{\"id\":%d}", g_Config.clsnd.id);
		if (!hs_data) {
			return NULL;
		}

		if (!sockaddrEncode(&saddr.sa, ipstrFamily(clsnd->ip), clsnd->ip, clsnd->port)) {
			free(hs_data);
			return NULL;
		}
		o = reactorobjectOpen(INVALID_FD_HANDLE, saddr.sa.sa_family, clsnd->socktype, 0);
		if (!o) {
			free(hs_data);
			return NULL;
		}
		channel = openChannelInner(o, CHANNEL_FLAG_CLIENT, &saddr.sa, dq);
		if (!channel) {
			reactorCommitCmd(NULL, &o->freecmd);
			free(hs_data);
			return NULL;
		}
		clsnd->connection_num++;
		sessionChannelReplaceClient(&clsnd->session, channel);
		reactorCommitCmd(selectReactor(), &o->regcmd);
		// handshake
		makeInnerMsg(&msg, 0, hs_data, hs_datalen)->rpc_status = RPC_STATUS_HAND_SHAKE;
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		free(hs_data);
	}
	return channel;
}

#ifdef __cplusplus
}
#endif