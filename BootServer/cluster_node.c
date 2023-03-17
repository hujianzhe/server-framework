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

int clsndSendv(ClusterNode_t* clsnd, const Iobuf_t iov[], unsigned int iovcnt) {
	ChannelBase_t* c;
	if (0 == strcmp(clsnd->ident, ptrBSG()->conf->clsnd.ident)) {
		return 0;
	}
	c = connectClusterNode(clsnd);
	if (!c) {
		return 0;
	}
	channelbaseSendv(c, iov, iovcnt, NETPACKET_FRAGMENT);
	return 1;
}

#ifdef __cplusplus
}
#endif

ChannelBase_t* connectClusterNode(ClusterNode_t* clsnd) {
	ChannelBase_t* channel;
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
		TaskThread_t* thrd;

		if (session->reconnect_timestamp_sec > 0 &&
			time(NULL) < session->reconnect_timestamp_sec)
		{
			return NULL;
		}

		thrd = currentTaskThread();
		if (!thrd) {
			return NULL;
		}

		if (session->do_connect_handshake) { /* user self-defining connect-handshake action */
			return session->do_connect_handshake(session, clsnd->ip, clsnd->port);
		}

		hs_data = strFormat(&hs_datalen, "{\"ident\":\"%s\"}", self_ident);
		if (!hs_data) {
			return NULL;
		}

		if (!sockaddrEncode(&saddr.sa, ipstrFamily(clsnd->ip), clsnd->ip, clsnd->port)) {
			free(hs_data);
			return NULL;
		}
		channel = openChannelInner(CHANNEL_FLAG_CLIENT, INVALID_FD_HANDLE, clsnd->socktype, &saddr.sa, thrd->sche);
		if (!channel) {
			free(hs_data);
			return NULL;
		}
		clsnd->connection_num++;
		sessionReplaceChannel(session, channel);
		channelbaseReg(selectReactor(), channel);
		/* default handshake */
		makeInnerMsg(&msg, 0, hs_data, hs_datalen)->rpc_status = RPC_STATUS_HAND_SHAKE;
		channelbaseSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		free(hs_data);
	}
	return channel;
}
