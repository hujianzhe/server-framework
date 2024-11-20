#include "global.h"
#include "dispatch_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

DispatchNetMsg_t* newDispatchNetMsg(size_t datalen, socklen_t saddrlen) {
	DispatchNetMsg_t* msg = (DispatchNetMsg_t*)malloc(sizeof(DispatchNetMsg_t) + datalen);
	if (msg) {
		if (saddrlen > 0) {
			msg->peer_addr = (struct sockaddr*)malloc(saddrlen);
			if (!msg->peer_addr) {
				free(msg);
				return NULL;
			}
			msg->peer_addr->sa_family = AF_UNSPEC;
			msg->peer_addrlen = saddrlen;
		}
		else {
			msg->peer_addr = NULL;
			msg->peer_addrlen = 0;
		}
		msg->rpcid = 0;
		msg->on_free = NULL;
		msg->param.type = 0;
		msg->param.value = NULL;
		msg->enqueue_time_msec = -1;
		msg->callback = NULL;
		msg->channel = NULL;
		msg->retcode = 0;
		msg->cmd = 0;
		msg->datalen = datalen;
		msg->data[msg->datalen] = 0;
	}
	return msg;
}

void freeDispatchNetMsg(DispatchNetMsg_t* msg) {
	if (!msg) {
		return;
	}
	if (msg->on_free) {
		msg->on_free(msg);
	}
	free(msg->peer_addr);
	free(msg);
}

#ifdef __cplusplus
}
#endif
