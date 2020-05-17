#ifndef MQ_HANDLER_H
#define	MQ_HANDLER_H

extern void unknowRequest(UserMsg_t* ctrl);

extern void frpc_test_code(Channel_t*);
extern void arpc_test_code(Channel_t*);
extern void reqTest(UserMsg_t*);
extern void notifyTest(UserMsg_t*);
extern void rpcRetTest(RpcItem_t*);
extern void retTest(UserMsg_t*);
extern void reqHttpTest(UserMsg_t*);
extern void reqSoTest(UserMsg_t* ctrl);

extern void reqUploadCluster(UserMsg_t*);
extern void retUploadCluster(UserMsg_t*);

#endif