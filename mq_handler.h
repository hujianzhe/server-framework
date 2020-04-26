#ifndef MQ_HANDLER_H
#define	MQ_HANDLER_H

extern void frpc_test_code(Session_t*);
extern void arpc_test_code(Session_t*);
extern int reqTest(UserMsg_t*);
extern int notifyTest(UserMsg_t*);
extern void rpcRetTest(RpcItem_t*);
extern int retTest(UserMsg_t*);
extern int reqHttpTest(UserMsg_t*);

extern int reqReconnectCluster(UserMsg_t*);
extern int retReconnect(UserMsg_t*);
extern int reqUploadCluster(UserMsg_t*);
extern int retUploadCluster(UserMsg_t*);
extern int notifyNewCluster(UserMsg_t*);
extern int reqRemoveCluster(UserMsg_t*);
extern int retRemoveCluster(UserMsg_t*);


#endif