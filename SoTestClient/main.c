#include "../BootServer/global.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

extern int init(TaskThread_t*, int, char**);
extern void destroy(TaskThread_t*);

static void sigintHandler(int signo) {
	stopBootServerGlobal();
}

int main(int argc, char** argv) {
	if (argc < 2) {
		fputs("need a config file to boot ...", stderr);
		return 1;
	}
	// reg SIGINT signal
	if (signalRegHandler(SIGINT, sigintHandler) == SIG_ERR) {
		fputs("signalRegHandler(SIGINT) failure", stderr);
		return 1;
	}
	// int BootServerGlobal
	if (!initBootServerGlobal(argv[1])) {
		fprintf(stderr, "initBootServerGlobal err:%s\n", getBSGErrmsg());
		return 1;
	}
	// print boot cluster node info
	printBootServerNodeInfo();
	// run BootServer and wait BootServer end
	runBootServerGlobal(argc, argv, init, destroy);
	// print BootServer run error
	if (getBSGErrmsg()) {
		fputs(getBSGErrmsg(), stderr);
		logErr(ptrBSG()->log, getBSGErrmsg());
	}
	// free BootServer
	freeBootServerGlobal();
	return 0;
}
