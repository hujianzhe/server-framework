#ifndef MQ_HANDLER_H
#define	MQ_HANDLER_H

extern int reqTest(MQRecvMsg_t*);
extern int notifyTest(MQRecvMsg_t*);
extern void rpcRetTest(RpcItem_t*);
extern int retTest(MQRecvMsg_t*);

extern int reqReconnectCluster(MQRecvMsg_t*);
extern int retReconnect(MQRecvMsg_t*);
extern int reqUploadCluster(MQRecvMsg_t*);
extern int retUploadCluster(MQRecvMsg_t*);
extern int notifyNewCluster(MQRecvMsg_t*);
extern int reqRemoveCluster(MQRecvMsg_t*);
extern int retRemoveCluster(MQRecvMsg_t*);


#endif