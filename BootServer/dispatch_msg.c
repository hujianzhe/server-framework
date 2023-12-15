#include "global.h"
#include "dispatch_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

DispatchNetMsg_t* newDispatchNetMsg(ChannelBase_t* channel, size_t datalen, void(*on_free)(DispatchBaseMsg_t*)) {
	DispatchNetMsg_t* msg = (DispatchNetMsg_t*)malloc(sizeof(DispatchNetMsg_t) + datalen);
	if (msg) {
		msg->base.dispatch_net_msg_type = 1;
		msg->base.rpcid = 0;
		msg->base.on_free = on_free;

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

void freeDispatchNetMsg(DispatchBaseMsg_t* msg) {
	DispatchNetMsg_t* net_msg;
	if (!msg) {
		return;
	}
	net_msg = pod_container_of(msg, DispatchNetMsg_t, base);
	free(net_msg);
}

#ifdef __cplusplus
}
#endif
