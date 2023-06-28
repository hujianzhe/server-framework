#ifndef TEST_HANDLER_H
#define	TEST_HANDLER_H

extern void reqEcho(TaskThread_t*, DispatchNetMsg_t*);
extern void reqTestCallback(TaskThread_t*, DispatchNetMsg_t*);
extern void reqTest(TaskThread_t*, DispatchNetMsg_t*);
extern void reqHttpTest(TaskThread_t*, DispatchNetMsg_t*);
extern void reqSoTest(TaskThread_t*, DispatchNetMsg_t*);
extern void reqTestExecQueue(TaskThread_t*, DispatchNetMsg_t*);
extern void reqClearExecQueue(TaskThread_t*, DispatchNetMsg_t*);
extern void reqParallelTest1(TaskThread_t*, DispatchNetMsg_t*);
extern void reqParallelTest2(TaskThread_t*, DispatchNetMsg_t*);
extern void reqHttpUploadFile(TaskThread_t*, DispatchNetMsg_t*);

extern void reqLoginTest(TaskThread_t*, DispatchNetMsg_t*);

#endif
