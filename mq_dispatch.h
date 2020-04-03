#ifndef MQ_DISPATCH_H
#define	MQ_DISPATCH_H

struct UserMsg_t;
typedef int(*DispatchCallback_t)(struct UserMsg_t*);

int initDispatch(void);
int regDispatchCallback(int cmd, DispatchCallback_t func);
DispatchCallback_t getDispatchCallback(int cmd);
void freeDispatchCallback(void);

#endif // !MQ_DISPATCH_H
