#include "global.h"
#include "dispatch_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

DispatchNetMsg_t* newDispatchNetMsg(NetChannel_t* channel, size_t datalen, void(*on_free)(DispatchNetMsg_t*)) {
	DispatchNetMsg_t* msg = (DispatchNetMsg_t*)malloc(sizeof(DispatchNetMsg_t) + datalen);
	if (msg) {
		msg->rpcid = 0;
		msg->on_free = on_free;

		msg->param.type = 0;
		msg->param.value = NULL;
		msg->enqueue_time_msec = -1;
		msg->callback = NULL;
		msg->channel = channel;
		msg->peer_addr.sa.sa_family = AF_UNSPEC;
		msg->peer_addrlen = 0;
		msg->retcode = 0;
		msg->datalen = datalen;
		msg->data[msg->datalen] = 0;
	}
	return msg;
}

void freeDispatchNetMsg(DispatchNetMsg_t* msg) {
	free(msg);
}

#ifdef __cplusplus
}
#endif
