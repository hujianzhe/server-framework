#ifndef GLOBAL_H
#define	GLOBAL_H

#include "util/inc/all.h"
#include "channel_imp.h"
#include "cluster.h"
#include "dispatch.h"
#include "msg_struct.h"
#include "rpc_helper.h"
#include "session_struct.h"
#include <stdlib.h>

extern int g_MainArgc;
extern char** g_MainArgv;
extern void* g_ModulePtr;
extern int(*g_ModuleInitFunc)(int, char**);
extern volatile int g_Valid;
extern Thread_t* g_ReactorThreads;
extern Thread_t* g_ReactorAcceptThread;
extern Thread_t g_TaskThread;
extern Reactor_t* g_Reactors;
extern Reactor_t* g_ReactorAccept;
extern DataQueue_t g_DataQueue;
extern size_t g_ReactorCnt;
extern RBTimer_t g_Timer;

#ifdef __cplusplus
extern "C" {
#endif

int initGlobalResource(void);
void freeGlobalResource(void);
__declspec_dllexport void g_Invalid(void);
__declspec_dllexport Reactor_t* selectReactor(size_t key);
__declspec_dllexport Reactor_t* ptr_g_ReactorAccept(void);
__declspec_dllexport DataQueue_t* ptr_g_DataQueue(void);
__declspec_dllexport RBTimer_t* ptr_g_Timer(void);

#ifdef __cplusplus
}
#endif

#endif // !GLOBAL_H
