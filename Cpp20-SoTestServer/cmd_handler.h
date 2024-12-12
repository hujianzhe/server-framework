#ifndef	CMD_HANDLER_H
#define	CMD_HANDLER_H

#include "../BootServer/dispatch.h"
#include "../BootServer/cpp_coroutine_sche.h"

namespace CmdHandler {
void reg_dispatch(Dispatch_t* dispatch);

util::CoroutinePromise<void> reqLoginTest(TaskThread_t* thrd, DispatchNetMsg_t* ctrl);
util::CoroutinePromise<void> reqParallelTest1(TaskThread_t* thrd, DispatchNetMsg_t* ctrl);
util::CoroutinePromise<void> reqParallelTest2(TaskThread_t* thrd, DispatchNetMsg_t* ctrl);
util::CoroutinePromise<void> reqTest(TaskThread_t* thrd, DispatchNetMsg_t* ctrl);
util::CoroutinePromise<void> reqEcho(TaskThread_t* thrd, DispatchNetMsg_t* ctrl);
util::CoroutinePromise<void> reqTestCallback(TaskThread_t* thrd, DispatchNetMsg_t* ctrl);
}

#endif
