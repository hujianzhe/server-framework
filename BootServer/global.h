#ifndef GLOBAL_H
#define	GLOBAL_H

#include "util/inc/all.h"
#include "channel_imp.h"
#include "cluster_node.h"
#include "cluster.h"
#include "dispatch.h"
#include "msg_struct.h"
#include "rpc_helper.h"
#include "session_struct.h"
#include "work_thread.h"
#include <stdlib.h>
#include <string.h>

extern int g_MainArgc;
extern char** g_MainArgv;
extern void* g_ModulePtr;
extern int(*g_ModuleInitFunc)(TaskThread_t*, int, char**);
extern volatile int g_Valid;
extern Thread_t* g_ReactorThreads;
extern Thread_t* g_ReactorAcceptThread;
extern Reactor_t* g_Reactors;
extern Reactor_t* g_ReactorAccept;
extern size_t g_ReactorCnt;
extern Log_t g_Log;

#ifdef __cplusplus
extern "C" {
#endif

int initGlobalResource(void);
void freeGlobalResource(void);
__declspec_dllexport void g_Invalid(void);
__declspec_dllexport Reactor_t* selectReactor(size_t key);
__declspec_dllexport Reactor_t* ptr_g_ReactorAccept(void);
__declspec_dllexport Log_t* ptr_g_Log(void);

#ifdef __cplusplus
}
#endif

#endif // !GLOBAL_H
