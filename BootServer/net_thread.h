#ifndef NET_THREAD_H
#define	NET_THREAD_H

#include "util/inc/component/reactor.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport Reactor_t* ptr_g_ReactorAccept(void);
__declspec_dllexport Reactor_t* selectReactor(size_t key);

int newNetThreadResource(void);
void freeNetThreadResource(void);
BOOL runNetThreads(void);
void wakeupNetThreads(void);
void joinNetThreads(void);

#ifdef __cplusplus
}
#endif

#endif // !NET_THREAD_H
