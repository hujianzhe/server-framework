#ifndef MQ_DISPATCH_H
#define	MQ_DISPATCH_H

struct MQRecvMsg_t;
typedef int(*MQDispatchCallback_t)(struct MQRecvMsg_t*);

int initDispatch(void);
int regDispatchCallback(int cmd, MQDispatchCallback_t func);
MQDispatchCallback_t getDispatchCallback(int cmd);
void freeDispatchCallback(void);

#endif // !MQ_DISPATCH_H
