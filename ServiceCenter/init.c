#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "../ServiceCommCode/service_comm_proc.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#pragma comment(lib, "ServiceCommCode.lib")
#endif

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(TaskThread_t* thrd, int argc, char** argv) {
	return 1;
}

#ifdef __cplusplus
}
#endif