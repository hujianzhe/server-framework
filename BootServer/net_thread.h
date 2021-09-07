#ifndef BOOT_SERVER_NET_THREAD_H
#define	BOOT_SERVER_NET_THREAD_H

#include "util/inc/component/reactor.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll Reactor_t* acceptReactor(void);
__declspec_dll Reactor_t* targetReactor(size_t key);
__declspec_dll Reactor_t* selectReactor(void);

int newNetThreadResource(unsigned int cnt);
void freeNetThreadResource(void);
BOOL runNetThreads(void);
void wakeupNetThreads(void);
void joinNetThreads(void);

#ifdef __cplusplus
}
#endif

#endif // !NET_THREAD_H
