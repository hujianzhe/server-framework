#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "../BootServer/cpp_coroutine_sche.h"
#include "test_handler.h"
#include <iostream>
#include <memory>

static util::CoroutinePromise<void> run(const std::any& param) {
	TaskThread_t* thrd = currentTaskThread();
	// listen extra port
	for (int i = 0; i < ptrBSG()->conf->listen_options_cnt; ++i) {
		const ConfigListenOption_t* option = ptrBSG()->conf->listen_options + i;
		std::unique_ptr<NetChannel_t, void(*)(NetChannel_t*)> c(nullptr, NetChannel_close_ref);
		if (!strcmp(option->protocol, "http")) {
			c.reset(openNetListenerHttp(option->ip, option->port, thrd->sche));
		}
		else {
			continue;
		}
		if (!c) {
			logErr(ptrBSG()->log, "listen failure, ip:%s, port:%u ......", option->ip, option->port);
			co_return;
		}
		NetChannel_reg(acceptNetReactor(), c.get());
	}
	co_return;
}

static util::CoroutinePromise<void> net_dispatch(TaskThread_t* thrd, DispatchNetMsg_t* net_msg) {
	CppCoroutineDispatchNetCallback fn = (CppCoroutineDispatchNetCallback)net_msg->callback;
	co_await fn(thrd, net_msg);
	co_return;
}

int init(void) {
	TaskThreadCppCoroutine* default_task_thread = (TaskThreadCppCoroutine*)ptrBSG()->default_task_thread;
	// register dispatch
	TestHandler::reg_dispatch(ptrBSG()->dispatch);

	default_task_thread->net_dispatch = net_dispatch;
	auto sc = (util::CoroutineDefaultSche*)default_task_thread->sche;
	sc->readyExec(run);
	return 0;
}
