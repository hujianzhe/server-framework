#include "test_handler.h"
#include "../BootServer/global.h"
#include <stdio.h>

namespace TestHandler {
void reg_dispatch(Dispatch_t* dispatch) {
	regStringDispatch(dispatch, "/reqSoTest", (DispatchNetCallback_t)reqSoTest);
	regStringDispatch(dispatch, "/reqSoTestEx", (DispatchNetCallback_t)reqSoTestEx);
}

util::CoroutinePromise<void> reqSoTest(TaskThread_t* thrd, DispatchNetMsg_t* req_ctrl) {
	HttpFrame_t* httpframe = (HttpFrame_t*)req_ctrl->param.value;
	printf("module recv http browser ... %s\n", httpframe->query);

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
		co_return;
	}
	NetChannel_send(req_ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT, NULL, 0);
	NetChannel_send_fin(req_ctrl->channel);
	free(reply);
	co_return;
}

util::CoroutinePromise<void> reqSoTestEx(TaskThread_t* thrd, DispatchNetMsg_t* req_ctrl) {
    auto sc = util::CoroutineDefaultSche::get();
	HttpFrame_t* httpframe = (HttpFrame_t*)req_ctrl->param.value;
	printf("reqSoTestEx module recv http browser ... %s\n", httpframe->query);

	util::CoroutineDefaultSche::LockGuard lg;
    co_await lg.lock("reqSoTestEx");
	co_await sc->sleepTimeout(5000);

	const char test_data[] = "C so/dll server say hello world exexexexex, yes ~.~";
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
		co_return;
	}
	NetChannel_send(req_ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT, NULL, 0);
	NetChannel_send_fin(req_ctrl->channel);
	free(reply);
	co_return;
}
}
