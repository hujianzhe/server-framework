#ifndef BOOT_SERVER_GLOBAL_H
#define	BOOT_SERVER_GLOBAL_H

#include "util/inc/all.h"
#include "config.h"
#include "channel_proc_imp.h"
#include "channel_inner.h"
#include "channel_http.h"
#include "channel_websocket.h"
#include "channel_redis.h"
#include "cluster_node.h"
#include "dispatch.h"
#include "dispatch_msg.h"
#include "inner_msg_struct.h"
#include "net_thread.h"
#include "task_thread.h"
#include <stdlib.h>
#include <string.h>

typedef struct BootServerGlobal_t {
	int argc;
	char** argv;
	volatile int valid;
	struct Log_t* log;
	const Config_t* conf;
	TaskThread_t* default_task_thread;
	const char* errmsg;
	struct Dispatch_t* dispatch;
} BootServerGlobal_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll BootServerGlobal_t* ptrBSG(void);
__declspec_dll const char* getBSGErrmsg(void);
__declspec_dll int checkStopBSG(void);

__declspec_dll BOOL initBootServerGlobal(const char* conf_path, int argc, char** argv, int(*fn_init)(BootServerGlobal_t*));
__declspec_dll void printBootServerNodeInfo(void);
__declspec_dll BOOL runBootServerGlobal(void);
__declspec_dll void stopBootServerGlobal(void);
__declspec_dll void freeBootServerGlobal(void);

#ifdef __cplusplus
}
#endif

#endif // !GLOBAL_H
