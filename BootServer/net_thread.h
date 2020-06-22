#ifndef NET_THREAD_H
#define	NET_THREAD_H

#include "util/inc/component/reactor.h"

extern Thread_t* g_ReactorThreads;
extern Thread_t* g_ReactorAcceptThread;
extern Reactor_t* g_Reactors;
extern Reactor_t* g_ReactorAccept;
extern size_t g_ReactorCnt;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport Reactor_t* ptr_g_ReactorAccept(void);

int newNetThreadResource(void);
void freeNetThreadResource(void);
__declspec_dllexport Reactor_t* selectReactor(size_t key);

#ifdef __cplusplus
}
#endif

#endif // !NET_THREAD_H
