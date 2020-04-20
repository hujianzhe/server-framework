#include "global.h"

int reqTest(UserMsg_t* ctrl) {
	char test_data[] = "this text is from server ^.^";
	SendMsg_t msg;

	printf("say hello world !!! %s\n", (char*)ctrl->data);

	makeSendMsg(&msg, CMD_NOTIFY_TEST, NULL, 0);
	channelShardSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);

	if (ctrl->rpc_status == 'R') {
		makeSendMsgRpcResp(&msg, ctrl->rpcid, test_data, sizeof(test_data));
	}
	else {
		makeSendMsg(&msg, CMD_RET_TEST, test_data, sizeof(test_data));
	}
	channelShardSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	return 0;
}

int notifyTest(UserMsg_t* ctrl) {
	printf("recv server test notify, recv msec = %lld\n", gmtimeMillisecond());
	return 0;
}

void rpcRetTest(RpcItem_t* rpc_item) {
	UserMsg_t* ctrl = (UserMsg_t*)rpc_item->ret_msg;
	long long cost_msec = gmtimeMillisecond() - rpc_item->timestamp_msec;
	printf("time cost(%lld msec) say hello world ... %s\n", cost_msec, ctrl->data);
}

int retTest(UserMsg_t* ctrl) {
	printf("say hello world ... %s, recv msec = %lld\n", ctrl->data, gmtimeMillisecond());
	return 0;
}