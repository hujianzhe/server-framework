#include "cmd_handler.h"
#include "cmd.h"
#include "../BootServer/global.h"
#include <iostream>
#include <stdio.h>

namespace CmdHandler {
void reg_dispatch(Dispatch_t* dispatch) {
	regNumberDispatch(dispatch, CMD_REQ_LOGIN_TEST, (DispatchNetCallback_t)reqLoginTest);
	regNumberDispatch(dispatch, CMD_REQ_ParallelTest1, (DispatchNetCallback_t)reqParallelTest1);
	regNumberDispatch(dispatch, CMD_REQ_ParallelTest2, (DispatchNetCallback_t)reqParallelTest2);
	regNumberDispatch(dispatch, CMD_REQ_TEST, (DispatchNetCallback_t)reqTest);
	regNumberDispatch(dispatch, CMD_REQ_ECHO, (DispatchNetCallback_t)reqEcho);
	regNumberDispatch(dispatch, CMD_REQ_TEST_CALLBACK, (DispatchNetCallback_t)reqTestCallback);
}

util::CoroutinePromise<void> reqLoginTest(TaskThread_t* thrd, DispatchNetMsg_t* ctrl) {
	InnerMsgPayload_t ret_msg;

	makeInnerMsg(&ret_msg, CMD_RET_LOGIN_TEST, NULL, 0);
	NetChannel_sendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	co_return;
}

util::CoroutinePromise<void> reqEcho(TaskThread_t* thrd, DispatchNetMsg_t* ctrl) {
	InnerMsgPayload_t msg;
	makeInnerMsgRpcResp(&msg, ctrl->rpcid, 0, ctrl->data, ctrl->datalen);
	NetChannel_sendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	puts("echo");
	co_return;
}

util::CoroutinePromise<void> reqTestCallback(TaskThread_t* thrd, DispatchNetMsg_t* ctrl) {
	char test_data[] = "your callback is from server ^.^";
	InnerMsgPayload_t msg;
	makeInnerMsgRpcResp(&msg, ctrl->rpcid, 0, test_data, sizeof(test_data));
	NetChannel_sendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	co_return;
}

util::CoroutinePromise<void> reqTest(TaskThread_t* thrd, DispatchNetMsg_t* ctrl) {
	char test_data[] = "this text is from server ^.^";
	InnerMsgPayload_t msg;

	printf("say hello world !!! %s\n", (char*)ctrl->data);

	makeInnerMsg(&msg, CMD_NOTIFY_TEST, NULL, 0);
	NetChannel_sendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);

	makeInnerMsgRpcResp(&msg, ctrl->rpcid, 0, test_data, sizeof(test_data));
	NetChannel_sendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	co_return;
}

util::CoroutinePromise<void> reqParallelTest1(TaskThread_t* thrd, DispatchNetMsg_t* ctrl) {
	const char reply[] = "reqParallelTest1";

	printf("%s hello world !!! %s\n", __FUNCTION__, (char*)ctrl->data);

	InnerMsgPayload_t msg;
	makeInnerMsgRpcResp(&msg, ctrl->rpcid, 0, reply, sizeof(reply));
	NetChannel_sendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	co_return;
}

util::CoroutinePromise<void> reqParallelTest2(TaskThread_t* thrd, DispatchNetMsg_t* ctrl) {
	const char reply[] = "reqParallelTest2";

	printf("say hello world !!! %s\n", (char*)ctrl->data);

	InnerMsgPayload_t msg;
	makeInnerMsgRpcResp(&msg, ctrl->rpcid, 0, reply, sizeof(reply));
	NetChannel_sendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	co_return;
}
}
