#include "../BootServer/global.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
static int s_exit_signo = SIGINT;
#else
static int s_exit_signo = SIGTERM;
#endif

extern int init(BootServerGlobal_t* g);

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
	if (argc < 2) {
		fputs("need a config file to boot ...", stderr);
		return 1;
	}
	// int BootServerGlobal
	if (!initBootServerGlobal(argv[1], argc, argv, init)) {
		fprintf(stderr, "initBootServerGlobal err:%s\n", getBSGErrmsg());
		return 1;
	}
	// reg signal
	if (!signalThreadMaskNotify()) {
		fprintf(stderr, "main thread signalThreadMaskNotify err:%d\n", errnoGet());
		goto err;
	}
	signalReg(s_exit_signo);
	if (!runBootServerSignalHandler(sig_proc)) {
		goto err;
	}
	// print boot cluster node info
	printBootServerNodeInfo();
	// run BootServer and wait BootServer end
	if (!runBootServerGlobal()) {
		goto err;
	}
	goto ret;
err:
	fputs(getBSGErrmsg(), stderr);
	logErr(ptrBSG()->log, "%s", getBSGErrmsg());
ret:
	// free BootServer
	freeBootServerGlobal();
	return 0;
}
