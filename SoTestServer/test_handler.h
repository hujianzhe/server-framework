#ifndef TEST_HANDLER_H
#define	TEST_HANDLER_H

extern void reqTestCallback(TaskThread_t*, UserMsg_t*);
extern void reqTest(TaskThread_t*, UserMsg_t*);
extern void reqHttpTest(TaskThread_t*, UserMsg_t*);
extern void reqSoTest(TaskThread_t*, UserMsg_t*);
extern void reqTestExecQueue(TaskThread_t*, UserMsg_t*);
extern void reqParallelTest1(TaskThread_t*, UserMsg_t*);
extern void reqParallelTest2(TaskThread_t*, UserMsg_t*);
extern void reqHttpUploadFile(TaskThread_t*, UserMsg_t*);

extern void reqLoginTest(TaskThread_t*, UserMsg_t*);

#endif
