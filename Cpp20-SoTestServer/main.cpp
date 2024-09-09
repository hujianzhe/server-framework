#include "../BootServer/global.h"
#include "../BootServer/cpp_coroutine_sche.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
static int s_exit_signo = SIGINT;
#else
static int s_exit_signo = SIGTERM;
#endif

extern int init(void);

static void sig_proc(int signo) {
	if (s_exit_signo == signo) {
		stopBootServerGlobal();
		return;
	}
	if (SIGINT == signo) {
		signalIdleHandler(signo);
		return;
	}
}

int main(int argc, char** argv) {
	int ret;
	BootServerConfig_t* bs_conf;
	if (argc < 2) {
		fputs("need a config file to boot ...", stderr);
		return 1;
	}
	/* parse config file */
	bs_conf = parseBootServerConfig(argv[1]);
	if (!bs_conf) {
		fputs("parseBootServerConfig error", stderr);
		return 1;
	}
	/* use cpp-20 stackless coroutine */
	TaskThread_t* default_task_thread = TaskThreadCppCoroutine::newInstance();
	/* init BootServer object */
	if (!initBootServerGlobal(bs_conf, default_task_thread, &NetScheHookCppCoroutine::hook)) {
		fprintf(stderr, "initBootServerGlobal err:%s\n", getBSGErrmsg());
		return 1;
	}
	ptrBSG()->argc = argc;
	ptrBSG()->argv = argv;
	/* reg signal */
	if (!signalThreadMaskNotify()) {
		fprintf(stderr, "main thread signalThreadMaskNotify err:%d\n", errnoGet());
		goto err;
	}
	signalReg(s_exit_signo);
	ptrBSG()->sig_proc = sig_proc;
	/* run your App init function */
	ret = init();
	if (ret) {
		fprintf(stderr, "App call init err, ret=%d\n", ret);
		goto err;
	}
	/* print boot cluster node info */
	printBootServerNodeInfo();
	/* run BootServer and wait BootServer end */
	if (!runBootServerGlobal()) {
		goto err;
	}
	goto ret;
err:
	fputs(getBSGErrmsg(), stderr);
	logErr(ptrBSG()->log, "%s", getBSGErrmsg());
ret:
	/* free BootServer object */
	freeBootServerGlobal();
	return 0;
}