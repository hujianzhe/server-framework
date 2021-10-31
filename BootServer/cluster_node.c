#include "global.h"
#include "cluster_node.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

ClusterNode_t* newClusterNode(int id, int socktype, const IPString_t ip, unsigned short port) {
	ClusterNode_t* clsnd = (ClusterNode_t*)malloc(sizeof(ClusterNode_t));
	if (clsnd) {
		initSession(&clsnd->session);
		clsnd->m_id_htnode.key.i32 = id;
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

Channel_t* connectClusterNode(ClusterNode_t* clsnd) {
	Channel_t* channel;
	int self_id = ptrBSG()->conf->clsnd.id;
	if (clsnd->id == self_id) {
		return NULL;
	}
	channel = sessionChannel(&clsnd->session);
	if (!channel) {
		InnerMsg_t msg;
		char* hs_data;
		int hs_datalen;
		Sockaddr_t saddr;
		ReactorObject_t* o;
		TaskThread_t* thrd;

		if (clsnd->session.reconnect_timestamp_sec > 0 &&
			time(NULL) < clsnd->session.reconnect_timestamp_sec)
		{
			return NULL;
		}

		thrd = currentTaskThread();
		if (!thrd) {
			return NULL;
		}

		hs_data = strFormat(&hs_datalen, "{\"id\":%d}", self_id);
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
		channel = openChannelInner(o, CHANNEL_FLAG_CLIENT, &saddr.sa, &thrd->dq);
		if (!channel) {
			reactorCommitCmd(NULL, &o->freecmd);
			free(hs_data);
			return NULL;
		}
		clsnd->connection_num++;
		sessionReplaceChannel(&clsnd->session, channel);
		reactorCommitCmd(selectReactor(), &o->regcmd);
		// handshake
		makeInnerMsg(&msg, 0, hs_data, hs_datalen)->rpc_status = RPC_STATUS_HAND_SHAKE;
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		free(hs_data);
	}
	return channel;
}

void clsndSendv(ClusterNode_t* clsnd, const Iobuf_t iov[], unsigned int iovcnt) {
	Channel_t* c;
	unsigned int i;
	if (clsnd->id == ptrBSG()->conf->clsnd.id) {
		return;
	}
	for (i = 0; i < iovcnt; ++i) {
		if (iobufLen(iov + i) > 0) {
			break;
		}
	}
	if (i == iovcnt) {
		return;
	}
	c = connectClusterNode(clsnd);
	if (!c) {
		return;
	}
	channelSendv(c, iov, iovcnt, NETPACKET_FRAGMENT);
}

#ifdef __cplusplus
}
#endif
