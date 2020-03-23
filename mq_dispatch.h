#ifndef MQ_DISPATCH_H
#define	MQ_DISPATCH_H

#include "util/inc/sysapi/process.h"

struct MQRecvMsg_t;
typedef int(*MQDispatchCallback_t)(struct MQRecvMsg_t*);

int initDispatch(void);
int regDispatchCallback(int cmd, MQDispatchCallback_t func);
MQDispatchCallback_t getDispatchCallback(int cmd);
void freeDispatchCallback(void);

int regDispatchRpcContext(int cmd, Fiber_t* fiber);
Fiber_t* getDispatchRpcContext(int cmd);

#endif // !MQ_DISPATCH_H
