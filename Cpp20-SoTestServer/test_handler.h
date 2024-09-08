#ifndef	TEST_HANDLER_H
#define	TEST_HANDLER_H

#include "../BootServer/dispatch.h"
#include "../BootServer/cpp_coroutine_sche.h"

namespace TestHandler {
void reg_dispatch(Dispatch_t* dispatch);

util::CoroutinePromise<void> reqSoTest(TaskThread_t* thrd, DispatchNetMsg_t* req_ctrl);
}

#endif
