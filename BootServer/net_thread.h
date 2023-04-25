#ifndef BOOT_SERVER_NET_THREAD_H
#define	BOOT_SERVER_NET_THREAD_H

#include "util/inc/component/reactor.h"

int newNetThreadResource(unsigned int cnt);
void freeNetThreadResource(void);
BOOL runNetThreads(void);
void wakeupNetThreads(void);
void joinNetThreads(void);

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll struct Reactor_t* acceptReactor(void);
__declspec_dll struct Reactor_t* targetReactor(size_t key);
__declspec_dll struct Reactor_t* selectReactor(void);

#ifdef __cplusplus
}
#endif

#endif // !NET_THREAD_H
