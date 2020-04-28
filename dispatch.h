#ifndef DISPATCH_H
#define	DISPATCH_H

struct UserMsg_t;
typedef int(*DispatchCallback_t)(struct UserMsg_t*);

int initDispatch(void);
int regStringDispatch(const char* str, DispatchCallback_t func);
int regNumberDispatch(int cmd, DispatchCallback_t func);
DispatchCallback_t getStringDispatch(const char* str);
DispatchCallback_t getNumberDispatch(int cmd);
void freeDispatchCallback(void);

#endif // !DISPATCH_H
