#ifndef BOOT_SERVER_GLOBAL_H
#define	BOOT_SERVER_GLOBAL_H

#include "util/inc/all.h"
#include "channel_imp.h"
#include "cluster_node.h"
#include "cluster.h"
#include "cluster_action.h"
#include "dispatch.h"
#include "msg_struct.h"
#include "net_thread.h"
#include "rpc_helper.h"
#include "session_struct.h"
#include "work_thread.h"
#include <stdlib.h>
#include <string.h>

extern int g_MainArgc;
extern char** g_MainArgv;
extern void* g_ModulePtr;
extern volatile int g_Valid;
extern Log_t g_Log;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport void g_Invalid(void);
__declspec_dllexport Log_t* ptr_g_Log(void);

#ifdef __cplusplus
}
#endif

#endif // !GLOBAL_H
