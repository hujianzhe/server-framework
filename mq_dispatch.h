#ifndef MQ_DISPATCH_H
#define	MQ_DISPATCH_H

struct Fiber;
struct MQRecvMsg_t;
typedef int(*MQDispatchCallback_t)(struct MQRecvMsg_t*);

int initDispatch(void);
int regDispatchCallback(int cmd, MQDispatchCallback_t func);
MQDispatchCallback_t getDispatchCallback(int cmd);
void freeDispatchCallback(void);

int regDispatchRpcContext(int cmd, struct Fiber_t* fiber);
struct Fiber_t* getDispatchRpcContext(int cmd);

#endif // !MQ_DISPATCH_H
