#include "global.h"
#include "dispatch_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

UserMsg_t* newUserMsg(size_t datalen) {
	UserMsg_t* msg = (UserMsg_t*)malloc(sizeof(UserMsg_t) + datalen);
	if (msg) {
		memset(&msg->serial, 0, sizeof(msg->serial));
		msg->channel = NULL;
		msg->peer_addr.sa.sa_family = AF_UNSPEC;
		msg->peer_addrlen = 0;
		msg->on_free = NULL;
		msg->param.type = 0;
		msg->param.value = NULL;
		msg->enqueue_time_msec = -1;
		msg->callback = NULL;
		msg->retcode = 0;
		msg->rpcid = 0;
		msg->datalen = datalen;
		msg->data[msg->datalen] = 0;
	}
	return msg;
}

void freeUserMsg(UserMsg_t* msg) {
	if (!msg) {
		return;
	}
	if (msg->on_free) {
		msg->on_free(msg);
	}
	free(msg);
}

void freeUserMsgSerial(SerialExecObj_t* serial) {
	UserMsg_t* msg = pod_container_of(serial, UserMsg_t, serial);
	if (msg->channel) {
		channelbaseClose(msg->channel);
	}
	freeUserMsg(msg);
}

#ifdef __cplusplus
}
#endif
