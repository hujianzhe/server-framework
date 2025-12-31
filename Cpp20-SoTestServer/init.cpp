#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "../BootServer/cpp_coroutine_sche.h"
#include "cmd_handler.h"
#include "test_handler.h"
#include <iostream>
#include <memory>

void init(void) {
	// init log
	for (unsigned int i = 0; i < ptrBSG()->conf->log_options_cnt; ++i) {
		const BootServerConfigLoggerOption_t* opt = ptrBSG()->conf->log_options + i;
		logEnableFile(ptrBSG()->log, opt->key, opt->base_path, logFileOutputOptionDefault(), logFileRotateOptionDefaultHour());
	}
	// register dispatch
	CmdHandler::reg_dispatch(ptrBSG()->dispatch);
	TestHandler::reg_dispatch(ptrBSG()->dispatch);
	// init2
	auto sc = (util::CoroutineDefaultSche*)ptrBSG()->default_task_thread->sche;
	sc->readyExec([]()->util::CoroutinePromise<void> {
		auto sc = util::CoroutineDefaultSche::get();
		TaskThread_t* thrd = currentTaskThread();
		// listen extra port
		for (int i = 0; i < ptrBSG()->conf->listen_options_cnt; ++i) {
			const BootServerConfigListenOption_t* option = ptrBSG()->conf->listen_options + i;
			std::unique_ptr<NetChannel_t, void(*)(NetChannel_t*)> c(nullptr, NetChannel_close_ref);
			if (!strcmp(option->protocol, "http")) {
				c.reset(openNetListenerHttp(option, thrd->sche));
			}
			else if (!strcmp(option->protocol, "inner")) {
				c.reset(openNetListenerInner(option, thrd->sche));
			}
			else {
				continue;
			}
			if (!c) {
				logError(ptrBSG()->log, "", "listen failure, ip:%s, port:%u ......", option->channel_opt.ip, option->channel_opt.port);
				co_return;
			}
			NetChannel_reg(acceptNetReactor(), c.get());
		}
		co_return;
	});
}
