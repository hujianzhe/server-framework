#include "global.h"

void frpc_test_code(Session_t* session) {
	// test code
	if (session->channel->_.flag & CHANNEL_FLAG_CLIENT) {
		RBTimerEvent_t* rpc_item_timeout_ev;
		RpcItem_t* rpc_item = (RpcItem_t*)malloc(sizeof(RpcItem_t) + sizeof(RBTimerEvent_t));
		if (!rpc_item) {
			return;
		}
		rpc_item_timeout_ev = (RBTimerEvent_t*)(rpc_item + 1);
		rpcItemSet(rpc_item, rpcGenId());
		if (!rpcFiberCoreRegItem(g_RpcFiberCore, rpc_item)) {
			printf("rpcid(%d) already send\n", rpc_item->id);
			free(rpc_item);
		}
		else {
			char test_data[] = "this text is from client ^.^";
			SendMsg_t msg;
			makeSendMsgRpcReq(&msg, CMD_REQ_TEST, rpc_item->id, test_data, sizeof(test_data));
			channelShardSendv(session->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);

			listPushNodeBack(&session->rpc_itemlist, &rpc_item->listnode);
			rpc_item->originator = session;
			rpc_item->timestamp_msec = gmtimeMillisecond();
			rpc_item->timeout_ev = rpc_item_timeout_ev;
			rpc_item_timeout_ev->timestamp_msec = rpc_item->timestamp_msec + 1000;
			rpc_item_timeout_ev->arg = rpc_item;
			rpc_item_timeout_ev->callback = (void*)1;
			rbtimerAddEvent(&g_TimerRpcTimeout, rpc_item_timeout_ev);

			rpc_item = rpcFiberCoreYield(g_RpcFiberCore);
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
}

void arpc_test_code(Session_t* session) {
	// test code
	if (session->channel->_.flag & CHANNEL_FLAG_CLIENT) {
		RBTimerEvent_t* rpc_item_timeout_ev;
		RpcItem_t* rpc_item = (RpcItem_t*)malloc(sizeof(RpcItem_t) + sizeof(RBTimerEvent_t));
		if (!rpc_item) {
			return;
		}
		rpc_item_timeout_ev = (RBTimerEvent_t*)(rpc_item + 1);
		rpcItemSet(rpc_item, rpcGenId());
		if (!rpcAsyncCoreRegItem(g_RpcAsyncCore, rpc_item, NULL, rpcRetTest)) {
			printf("rpcid(%d) already send\n", rpc_item->id);
			free(rpc_item);
		}
		else {
			char test_data[] = "this text is from client ^.^";
			SendMsg_t msg;
			makeSendMsgRpcReq(&msg, CMD_REQ_TEST, rpc_item->id, test_data, sizeof(test_data));
			channelShardSendv(session->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);

			listPushNodeBack(&session->rpc_itemlist, &rpc_item->listnode);
			rpc_item->originator = session;
			rpc_item->timestamp_msec = gmtimeMillisecond();
			rpc_item->timeout_ev = rpc_item_timeout_ev;
			rpc_item_timeout_ev->timestamp_msec = rpc_item->timestamp_msec + 1000;
			rpc_item_timeout_ev->arg = rpc_item;
			rpc_item_timeout_ev->callback = (void*)1;
			rbtimerAddEvent(&g_TimerRpcTimeout, rpc_item_timeout_ev);
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
	if (g_RpcFiberCore)
		frpc_test_code(session);
	else if (g_RpcAsyncCore)
		arpc_test_code(session);

	return 0;
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

int retTest(UserMsg_t* ctrl) {
	printf("say hello world ... %s, recv msec = %lld\n", ctrl->data, gmtimeMillisecond());
	return 0;
}

int reqHttpTest(UserMsg_t* ctrl) {
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
		return 0;
	}
	channelShardSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT);
	reactorCommitCmd(NULL, &ctrl->channel->_.stream_sendfincmd);
	free(reply);
	return 0;
}