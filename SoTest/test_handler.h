#ifndef TEST_HANDLER_H
#define	TEST_HANDLER_H

extern void frpc_test_code(TaskThread_t*, Channel_t*);
extern void arpc_test_code(TaskThread_t*, Channel_t*);
extern void reqTest(TaskThread_t*, UserMsg_t*);
extern void notifyTest(TaskThread_t*, UserMsg_t*);
extern void rpcRetTest(RpcAsyncCore_t*, RpcItem_t*);
extern void retTest(TaskThread_t*, UserMsg_t*);
extern void reqHttpTest(TaskThread_t*, UserMsg_t*);
extern void reqSoTest(TaskThread_t*, UserMsg_t*);
extern void reqWebsocketTest(TaskThread_t*, UserMsg_t*);
extern void reqParallelTest1(TaskThread_t*, UserMsg_t*);
extern void reqParallelTest2(TaskThread_t*, UserMsg_t*);

extern void reqLoginTest(TaskThread_t*, UserMsg_t*);
extern void retLoginTest(TaskThread_t*, UserMsg_t*);

#endif