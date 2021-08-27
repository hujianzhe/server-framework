#include "global.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_STATIC_MODULE
#ifdef __cplusplus
extern "C" {
#endif
	int init(TaskThread_t*, int, char**);
	void destroy(TaskThread_t*);
#ifdef __cplusplus
}
#endif
#endif

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
#ifdef USE_STATIC_MODULE
	runBootServerGlobal(argc, argv, init, destroy);
#else
	runBootServerGlobal(argc, argv, NULL, NULL);
#endif
	// print BootServer run error
	if (getBSGErrmsg()) {
		fputs(getBSGErrmsg(), stderr);
		logErr(ptrBSG()->log, getBSGErrmsg());
	}
	// free BootServer
	freeBootServerGlobal();
	return 0;
}
