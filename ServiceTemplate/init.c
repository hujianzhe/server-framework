#include "../BootServer/config.h"
#include "../BootServer/global.h"

void run(struct StackCoSche_t* sche, StackCoAsyncParam_t* param) {
	unsigned int i;
	// listen port
	for (i = 0; i < ptrBSG()->conf->listen_options_cnt; ++i) {
		const BootServerConfigListenOption_t* option = ptrBSG()->conf->listen_options + i;
		NetChannel_t* c;
		if (!strcmp(option->protocol, "http")) {
			c = openNetListenerHttp(option, sche);
		}
		else {
			continue;
		}
		if (!c) {
			logError(ptrBSG()->log, "", "listen failure, ip:%s, port:%u ......", option->ip, option->port);
			return;
		}
		NetChannel_reg(acceptNetReactor(), c);
		NetChannel_close_ref(c);
	}

	logInfo(ptrBSG()->log, "", "init ok ......");
}

int init(void) {
	// init log
	unsigned int i;
	for (i = 0; i < ptrBSG()->conf->log_options_cnt; ++i) {
		const BootServerConfigLoggerOption_t* opt = ptrBSG()->conf->log_options + i;
		logEnableFile(ptrBSG()->log, opt->key, opt->base_path, logFileOutputOptionDefault(), logFileRotateOptionDefaultHour());
	}

	StackCoSche_function(ptrBSG()->default_task_thread->sche_stack_co, run, NULL);
	return 0;
}
