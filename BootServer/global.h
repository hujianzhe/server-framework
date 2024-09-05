#ifndef BOOT_SERVER_GLOBAL_H
#define	BOOT_SERVER_GLOBAL_H

#include "util/inc/all.h"
#include "config.h"
#include "net_channel_proc_imp.h"
#include "net_channel_inner.h"
#include "net_channel_http.h"
#include "net_channel_websocket.h"
#include "net_channel_redis.h"
#include "cluster_node.h"
#include "dispatch.h"
#include "dispatch_msg.h"
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
	Thread_t sig_tid;
	void(*sig_proc)(int);
	const NetScheHook_t* net_sche_hook;
} BootServerGlobal_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll BootServerGlobal_t* ptrBSG(void);
__declspec_dll const char* getBSGErrmsg(void);

__declspec_dll BOOL initBootServerGlobal(const char* conf_path);
__declspec_dll void printBootServerNodeInfo(void);
__declspec_dll BOOL runBootServerGlobal(void);
__declspec_dll void stopBootServerGlobal(void);
__declspec_dll void freeBootServerGlobal(void);

#ifdef __cplusplus
}
#endif

#endif // !GLOBAL_H
