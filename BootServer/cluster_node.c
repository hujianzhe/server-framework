#include "global.h"
#include "cluster_node.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

ClusterNode_t* ClusterNode_constructor(ClusterNode_t* clsnd, const char* ident, int socktype, const IPString_t ip, unsigned short port) {
	clsnd->ident = strdup(ident);
	if (!clsnd->ident) {
		return NULL;
	}
	clsnd->m_ident_htnode.key.ptr = clsnd->ident;
	clsnd->socktype = socktype;
	if (ip) {
		strcpy(clsnd->ip, ip);
	}
	clsnd->port = port;
	clsnd->connection_num = 0;
	clsnd->status = CLSND_STATUS_NORMAL;
	clsnd->factor = 0;

	sessionInit(&clsnd->session);
	return clsnd;
}

void ClusterNode_destructor(ClusterNode_t* clsnd) {
	free((void*)clsnd->ident);
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
	if (!channel && session->do_connect_handshake) {
		/* user self-defining connect-handshake action */
		channel = session->do_connect_handshake(session, clsnd->socktype, clsnd->ip, clsnd->port);
	}
	return channel;
}
