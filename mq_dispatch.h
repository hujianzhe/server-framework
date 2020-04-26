#ifndef MQ_DISPATCH_H
#define	MQ_DISPATCH_H

struct UserMsg_t;
typedef int(*DispatchCallback_t)(struct UserMsg_t*);

int initDispatch(void);
int regStringDispatch(const char* str, DispatchCallback_t func);
int regNumberDispatch(int cmd, DispatchCallback_t func);
DispatchCallback_t getStringDispatch(const char* str);
DispatchCallback_t getNumberDispatch(int cmd);
void freeDispatchCallback(void);

#endif // !MQ_DISPATCH_H
