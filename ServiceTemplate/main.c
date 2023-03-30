#include "../BootServer/global.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

extern int init(BootServerGlobal_t* g);
extern void run(struct StackCoSche_t* sche, void* arg);

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
	if (!initBootServerGlobal(argv[1], argc, argv, init)) {
		fprintf(stderr, "initBootServerGlobal err:%s\n", getBSGErrmsg());
		return 1;
	}
	// print boot cluster node info
	printBootServerNodeInfo();
	// run BootServer and wait BootServer end
	runBootServerGlobal(run);
	// print BootServer run error
	if (getBSGErrmsg() && getBSGErrmsg()[0]) {
		fputs(getBSGErrmsg(), stderr);
		logErr(ptrBSG()->log, "%s", getBSGErrmsg());
	}
	// free BootServer
	freeBootServerGlobal();
	return 0;
}
