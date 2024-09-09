#include "test_handler.h"
#include "../BootServer/global.h"
#include <iostream>
#include <stdio.h>

namespace TestHandler {
void reg_dispatch(Dispatch_t* dispatch) {
	regStringDispatch(dispatch, "/reqSoTest", (DispatchNetCallback_t)reqSoTest);
	regStringDispatch(dispatch, "/reqSoTestEx", (DispatchNetCallback_t)reqSoTestEx);
	regStringDispatch(dispatch, "/reqSoTestEntry", (DispatchNetCallback_t)reqSoTestEntry);
}

util::CoroutinePromise<void> reqSoTest(TaskThread_t* thrd, DispatchNetMsg_t* req_ctrl) {
	HttpFrame_t* httpframe = (HttpFrame_t*)req_ctrl->param.value;
	std::cerr << "module recv http browser ... " << httpframe->query << std::endl;

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
	std::cerr << "reqSoTestEx module recv http browser ... " <<  httpframe->query << std::endl;

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

static util::CoroutinePromise<std::string> nothing(int tlen) {
    auto sc = util::CoroutineDefaultSche::get();
    if (tlen > 0) {
        co_await sc->sleepTimeout(tlen);
    }
    std::cout << sc->current_co_node() << " " << tlen << " nothing...\n";
    co_return "mlm";
}

util::CoroutinePromise<void> reqSoTestEntry(TaskThread_t* thrd, DispatchNetMsg_t* req_ctrl) {
	auto sc = util::CoroutineDefaultSche::get();
	util::CoroutineDefaultSche::LockGuard lg;

	co_await lg.lock("entry");
	std::cout << sc->current_co_node() << " entry run..." << std::endl;

	std::string s = co_await nothing(1000);
	std::cout << "input " << s << " sleep\n";
	co_await sc->sleepTimeout(1000);

	util::CoroutineAwaiterAnyone awaiter_any;
	util::CoroutinePromise<std::string> test_promise = nothing(1000);
	awaiter_any.addWithIdentity(test_promise, 0);
	awaiter_any.addWithIdentity(nothing(100), 1);
	while (!awaiter_any.allDone()) {
		util::CoroutineNode* co_node = co_await awaiter_any;
		std::cout << co_node->ident() << " finished co_node: " << co_node << " ret: " << co_node->ident() << std::endl;
	}

	std::cout << sc->current_co_node() << " entry exit..." << std::endl;

	const char test_data[] = "C so/dll server say hello world entry, yes ~.~";
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
