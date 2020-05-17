#ifndef TEST_HANDLER_H
#define	TEST_HANDLER_H

extern void unknowRequest(UserMsg_t* ctrl);

extern void frpc_test_code(Channel_t*);
extern void arpc_test_code(Channel_t*);
extern void reqTest(UserMsg_t*);
extern void notifyTest(UserMsg_t*);
extern void rpcRetTest(RpcItem_t*);
extern void retTest(UserMsg_t*);
extern void reqHttpTest(UserMsg_t*);
extern void reqSoTest(UserMsg_t* ctrl);

extern void reqLoginTest(UserMsg_t*);
extern void retLoginTest(UserMsg_t*);

#endif