#include "global.h"

void frpc_test_code(Session_t* session) {
	// test code
	if (session->channel->_.flag & CHANNEL_FLAG_CLIENT) {
		RpcItem_t* rpc_item = (RpcItem_t*)malloc(sizeof(RpcItem_t));
		if (!rpc_item) {
			return;
		}
		rpcItemSet(rpc_item, rpcGenId(), 1000);
		if (!rpcFiberCoreRegItem(session->f_rpc, rpc_item)) {
			printf("rpcid(%d) already send\n", rpc_item->id);
			free(rpc_item);
		}
		else {
			long long now_msec, cost_msec;
			char test_data[] = "this text is from client ^.^";
			SendMsg_t msg;
			makeSendMsgRpcReq(&msg, CMD_REQ_TEST, rpc_item->id, test_data, sizeof(test_data));
			channelShardSendv(session->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
			rpc_item->timestamp_msec = gmtimeMillisecond();
			rpc_item = rpcFiberCoreYield(session->f_rpc);

			now_msec = gmtimeMillisecond();
			cost_msec = now_msec - rpc_item->timestamp_msec;
			printf("rpc(%d) send msec=%lld time cost(%lld msec)\n", rpc_item->id, rpc_item->timestamp_msec, cost_msec);
			if (rpc_item->timeout_msec >= 0 && cost_msec >= rpc_item->timeout_msec) {
				printf("rpc(%d) call timeout\n", rpc_item->id);
			}
			else if (rpc_item->ret_msg) {
				UserMsg_t* ret_msg = (UserMsg_t*)rpc_item->ret_msg;
				printf("rpc(%d) say hello world ... %s\n", rpc_item->id, ret_msg->data);
			}
		}
	}
}

void arpc_test_code(Session_t* session) {
	// test code
	if (session->channel->_.flag & CHANNEL_FLAG_CLIENT) {
		RpcItem_t* rpc_item = (RpcItem_t*)malloc(sizeof(RpcItem_t));
		if (!rpc_item) {
			return;
		}
		rpcItemSet(rpc_item, rpcGenId(), 1000);
		if (!rpcAsyncCoreRegItem(session->a_rpc, rpc_item, NULL, rpcRetTest)) {
			printf("rpcid(%d) already send\n", rpc_item->id);
			free(rpc_item);
		}
		else {
			char test_data[] = "this text is from client ^.^";
			SendMsg_t msg;
			makeSendMsgRpcReq(&msg, CMD_REQ_TEST, rpc_item->id, test_data, sizeof(test_data));
			channelShardSendv(session->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
			rpc_item->timestamp_msec = gmtimeMillisecond();
			printf("rpc(%d) start send, send_msec=%lld\n", rpc_item->id, rpc_item->timestamp_msec);
		}
	}
}

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
	Session_t* session = (Session_t*)channelSession(ctrl->channel);
	printf("recv server test notify, recv msec = %lld\n", gmtimeMillisecond());
	// test code
	if (session->f_rpc)
		frpc_test_code(session);
	else if (session->a_rpc)
		arpc_test_code(session);

	return 0;
}

void rpcRetTest(RpcItem_t* rpc_item) {
	UserMsg_t* ret_msg = (UserMsg_t*)rpc_item->ret_msg;
	if (!ret_msg) {
		return;
	}
	else {
		Session_t* session = (Session_t*)channelSession(ret_msg->channel);
		long long cost_msec = gmtimeMillisecond() - rpc_item->timestamp_msec;
		printf("rpc(%d) send msec=%lld time cost(%lld msec)\n", rpc_item->id, rpc_item->timestamp_msec, cost_msec);
		printf("rpc(%d) say hello world ... %s\n", rpc_item->id, ret_msg->data);
	}
}

int retTest(UserMsg_t* ctrl) {
	printf("say hello world ... %s, recv msec = %lld\n", ctrl->data, gmtimeMillisecond());
	return 0;
}