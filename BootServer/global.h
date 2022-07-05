#ifndef BOOT_SERVER_GLOBAL_H
#define	BOOT_SERVER_GLOBAL_H

#include "util/inc/all.h"
#include "config.h"
#include "channel_proc_imp.h"
#include "channel_inner.h"
#include "channel_web.h"
#include "cluster_node.h"
#include "cluster_node_group.h"
#include "cluster.h"
#include "cluster_action.h"
#include "dispatch.h"
#include "msg_struct.h"
#include "net_thread.h"
#include "rpc_helper.h"
#include "session_struct.h"
#include "task_thread.h"
#include <stdlib.h>
#include <string.h>

typedef struct BootServerGlobal_t {
	int argc;
	char** argv;
	volatile int valid;
	Log_t* log;
	Config_t* conf;
	TaskThread_t* default_task_thread;
	const char* errmsg;
} BootServerGlobal_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll BootServerGlobal_t* ptrBSG(void);
__declspec_dll const char* getBSGErrmsg(void);
__declspec_dll int checkStopBSG(void);

__declspec_dll BOOL initBootServerGlobal(const char* conf_path);
__declspec_dll void printBootServerNodeInfo(void);
__declspec_dll BOOL runBootServerGlobal(int argc, char** argv, int(*fn_init)(TaskThread_t*, int, char**), void(*fn_destroy)(TaskThread_t*));
__declspec_dll void stopBootServerGlobal(void);
__declspec_dll void freeBootServerGlobal(void);

#ifdef __cplusplus
}
#endif

#endif // !GLOBAL_H
