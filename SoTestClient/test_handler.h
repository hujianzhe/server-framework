#ifndef TEST_HANDLER_H
#define	TEST_HANDLER_H

extern void frpc_test_code(TaskThread_t*, ChannelBase_t*);
extern void notifyTest(TaskThread_t*, UserMsg_t*);
extern void retTest(TaskThread_t*, UserMsg_t*);

extern void retLoginTest(TaskThread_t*, UserMsg_t*);

#endif
