#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"

static void frpc_callback(RpcItem_t* rpc_item) {
	if (rpc_item->ret_msg) {
		UserMsg_t* ret_msg = (UserMsg_t*)rpc_item->ret_msg;
		printf("rpc(%d) %s callback ... %s\n",
			rpc_item->id, (char*)rpc_item->async_req_arg, ret_msg->data);
	}
	else {
		printf("rpc callback(%d)..... failure timeout or cancel\n", rpc_item->id);
	}
}

void frpc_test_code(TaskThread_t* thrd, Channel_t* channel) {
	char test_data[] = "this text is from client ^.^";
	InnerMsg_t msg;
	RpcItem_t* rpc_item;
	//
	rpc_item = newRpcItemFiberReady(channel, 1000, (void*)"abcdefg", frpc_callback);
	if (!rpc_item) {
		return;
	}
	makeInnerMsgRpcReq(&msg, rpc_item->id, CMD_REQ_TEST_CALLBACK, test_data, sizeof(test_data));
	channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	//
	rpc_item = newRpcItemFiberReady(channel, 1000, NULL, NULL);
	if (!rpc_item) {
		return;
	}
	makeInnerMsgRpcReq(&msg, rpc_item->id, CMD_REQ_TEST, test_data, sizeof(test_data));
	channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	rpc_item = rpcFiberCoreYield(thrd->f_rpc);
	if (rpc_item->ret_msg) {
		UserMsg_t* ret_msg = (UserMsg_t*)rpc_item->ret_msg;
		long long cost_msec = gmtimeMillisecond() - rpc_item->timestamp_msec;
		printf("rpc(%d) send msec=%lld time cost(%lld msec)\n", rpc_item->id, rpc_item->timestamp_msec, cost_msec);
		printf("rpc(%d) say hello world ... %s\n", rpc_item->id, ret_msg->data);
	}
	else {
		printf("rpc(%d) call failure timeout or cancel\n", rpc_item->id);
		return;
	}
	/*
	rpc_item = newRpcItemFiberReady(channel, 1000, "abcdefg", frpc_callback);
	if (!rpc_item) {
		return;
	}
	makeInnerMsgRpcReq(&msg, rpc_item->id, CMD_REQ_TEST_CALLBACK, test_data, sizeof(test_data));
	channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	*/
}

void arpc_test_code(TaskThread_t* thrd, Channel_t* channel) {
	// test code
	char test_data[] = "this text is from client ^.^";
	InnerMsg_t msg;
	RpcItem_t* rpc_item = newRpcItemAsyncReady(channel, 1000, NULL, rpcRetTest);
	if (!rpc_item) {
		return;
	}
	makeInnerMsgRpcReq(&msg, rpc_item->id, CMD_REQ_TEST, test_data, sizeof(test_data));
	channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
}

void notifyTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	Session_t* session = channelSession(ctrl->channel);
	printf("recv server test notify, recv msec = %lld\n", gmtimeMillisecond());
	// test code
	if (thrd->f_rpc)
		frpc_test_code(thrd, ctrl->channel);
	else if (thrd->a_rpc)
		arpc_test_code(thrd, ctrl->channel);
}

void rpcRetTest(RpcItem_t* rpc_item) {
	UserMsg_t* ret_msg = (UserMsg_t*)rpc_item->ret_msg;
	if (!ret_msg) {
		puts("rpc call failure timeout or cancel");
		return;
	}
	else {
		long long cost_msec = gmtimeMillisecond() - rpc_item->timestamp_msec;
		printf("rpc(%d) send msec=%lld time cost(%lld msec)\n", rpc_item->id, rpc_item->timestamp_msec, cost_msec);
		printf("rpc(%d) say hello world ... %s\n", rpc_item->id, ret_msg->data);
	}
}

void retTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	printf("say hello world ... %s, recv msec = %lld\n", ctrl->data, gmtimeMillisecond());
}
