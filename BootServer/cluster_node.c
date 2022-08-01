#include "global.h"
#include "cluster_node.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

ClusterNode_t* newClusterNode(const char* ident, int socktype, const IPString_t ip, unsigned short port) {
	ClusterNode_t* clsnd = (ClusterNode_t*)malloc(sizeof(ClusterNode_t));
	if (!clsnd) {
		return NULL;
	}
	clsnd->ident = strdup(ident);
	if (!clsnd->ident) {
		free(clsnd);
		return NULL;
	}
	clsnd->m_ident_htnode.key.ptr = clsnd->ident;
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
	clsnd->factor = 0;
	initSession(&clsnd->session);
	return clsnd;
}

void freeClusterNode(ClusterNode_t* clsnd) {
	if (!clsnd) {
		return;
	}
	free((void*)clsnd->ident);
	free(clsnd);
}

Channel_t* connectClusterNode(ClusterNode_t* clsnd) {
	Channel_t* channel;
	Session_t* session;
	const char* self_ident = ptrBSG()->conf->clsnd.ident;
	if (0 == strcmp(clsnd->ident, self_ident)) {
		return NULL;
	}
	session = &clsnd->session;
	channel = sessionChannel(session);
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

		if (session->on_handshake) {
			hs_data = NULL;
		}
		else {
			hs_data = strFormat(&hs_datalen, "{\"ident\":\"%s\"}", self_ident);
			if (!hs_data) {
				return NULL;
			}
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
		if (session->on_handshake) {
			session->on_handshake(session, channel);
		}
		else {
			makeInnerMsg(&msg, 0, hs_data, hs_datalen)->rpc_status = RPC_STATUS_HAND_SHAKE;
			channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
			free(hs_data);
		}
	}
	return channel;
}

void clsndSendv(ClusterNode_t* clsnd, const Iobuf_t iov[], unsigned int iovcnt) {
	Channel_t* c;
	unsigned int i;
	if (0 == strcmp(clsnd->ident, ptrBSG()->conf->clsnd.ident)) {
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
