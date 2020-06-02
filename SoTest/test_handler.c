#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"

void frpc_test_code(TaskThread_t* thrd, Channel_t* channel) {
	// test code
	if (channel->_.flag & CHANNEL_FLAG_CLIENT) {
		char test_data[] = "this text is from client ^.^";
		SendMsg_t msg;
		RpcItem_t* rpc_item = newRpcItemFiberReady(thrd, channel, 1000);
		if (!rpc_item) {
			return;
		}
		makeSendMsgRpcReq(&msg, rpc_item->id, CMD_REQ_TEST, test_data, sizeof(test_data));
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		rpc_item = rpcFiberCoreYield(thrd->f_rpc);
		if (rpc_item->ret_msg) {
			UserMsg_t* ret_msg = (UserMsg_t*)rpc_item->ret_msg;
			long long cost_msec = gmtimeMillisecond() - rpc_item->timestamp_msec;
			printf("rpc(%d) send msec=%lld time cost(%lld msec)\n", rpc_item->id, rpc_item->timestamp_msec, cost_msec);
			printf("rpc(%d) say hello world ... %s\n", rpc_item->id, ret_msg->data);
		}
		else {
			puts("rpc call failure timeout or cancel");
		}
	}
}

void arpc_test_code(TaskThread_t* thrd, Channel_t* channel) {
	// test code
	if (channel->_.flag & CHANNEL_FLAG_CLIENT) {
		char test_data[] = "this text is from client ^.^";
		SendMsg_t msg;
		RpcItem_t* rpc_item = newRpcItemAsyncReady(thrd, channel, 1000, NULL, rpcRetTest);
		if (!rpc_item) {
			return;
		}
		makeSendMsgRpcReq(&msg, rpc_item->id, CMD_REQ_TEST, test_data, sizeof(test_data));
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	}
}

void reqTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	char test_data[] = "this text is from server ^.^";
	SendMsg_t msg;

	printf("say hello world !!! %s\n", (char*)ctrl->data);

	makeSendMsg(&msg, CMD_NOTIFY_TEST, NULL, 0);
	channelSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);

	if (ctrl->rpc_status == 'R') {
		makeSendMsgRpcResp(&msg, ctrl->rpcid, 0, test_data, sizeof(test_data));
	}
	else {
		makeSendMsg(&msg, CMD_RET_TEST, test_data, sizeof(test_data));
	}
	channelSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
}

void notifyTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	Session_t* session = (Session_t*)channelSession(ctrl->channel);
	printf("recv server test notify, recv msec = %lld\n", gmtimeMillisecond());
	// test code
	if (thrd->f_rpc)
		frpc_test_code(thrd, ctrl->channel);
	else if (thrd->a_rpc)
		arpc_test_code(thrd, ctrl->channel);
}

void rpcRetTest(RpcAsyncCore_t* rpc, RpcItem_t* rpc_item) {
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

void reqHttpTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	HttpFrame_t* httpframe = ctrl->httpframe;
	printf("recv http browser ... %s\n", httpframe->query);
	free(httpframeReset(httpframe));

	const char test_data[] = "C server say hello world, yes ~.~";
	int reply_len;
	char* reply = strFormat(&reply_len,
		"HTTP/1.1 %u %s\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Connection: close\r\n"
		"Content-Length:%u\r\n"
		"\r\n"
		"%s",
		200, httpframeStatusDesc(200), sizeof(test_data) - 1, test_data
	);
	if (!reply) {
		return;
	}
	channelSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT);
	reactorCommitCmd(NULL, &ctrl->channel->_.stream_sendfincmd);
	free(reply);
	return;
}

void reqSoTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	HttpFrame_t* httpframe = ctrl->httpframe;
	printf("module recv http browser ... %s\n", httpframe->query);
	free(httpframeReset(httpframe));

	const char test_data[] = "C so/dll server say hello world, yes ~.~";
	int reply_len;
	char* reply = strFormat(&reply_len,
		"HTTP/1.1 %u %s\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Connection: close\r\n"
		"Content-Length:%u\r\n"
		"\r\n"
		"%s",
		200, httpframeStatusDesc(200), sizeof(test_data) - 1, test_data
	);
	if (!reply) {
		return;
	}
	channelSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT);
	reactorCommitCmd(NULL, &ctrl->channel->_.stream_sendfincmd);
	free(reply);
}

void reqWebsocketTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	const char reply[] = "This text is from Server &.&, [reply websocket]";
	printf("%s recv: %s\n", __FUNCTION__, ctrl->data);
	channelSend(ctrl->channel, reply, strlen(reply), NETPACKET_FRAGMENT);
}