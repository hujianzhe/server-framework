#include "global.h"

int reqTest(MQRecvMsg_t* ctrl) {
	char test_data[] = "this text is from server ^.^";
	MQSendMsg_t msg;

	printf("say hello world !!! %s\n", (char*)ctrl->data);

	makeMQSendMsg(&msg, CMD_NOTIFY_TEST, NULL, 0);
	channelSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);

	makeMQSendMsg(&msg, CMD_RET_TEST, test_data, sizeof(test_data));
	channelSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	return 0;
}

int notifyTest(MQRecvMsg_t* ctrl) {
	printf("recv server test notify, recv msec = %lld\n", gmtimeMillisecond());
	return 0;
}

void rpcRetTest(RpcItem_t* rpc_item) {
	MQRecvMsg_t* ctrl = (MQRecvMsg_t*)rpc_item->ret_msg;
	long long cost_msec = gmtimeMillisecond() - rpc_item->timestamp_msec;
	printf("time cost(%lld msec) say hello world ... %s\n", cost_msec, ctrl->data);
}

int retTest(MQRecvMsg_t* ctrl) {
	printf("say hello world ... %s, recv msec = %lld\n", ctrl->data, gmtimeMillisecond());
	return 0;
}