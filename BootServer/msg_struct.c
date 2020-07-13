#include "msg_struct.h"
#include "util/inc/sysapi/socket.h"

#ifdef __cplusplus
extern "C" {
#endif

SendMsg_t* makeSendMsgEmpty(SendMsg_t* msg) {
	int i;
	for (i = 0; i < sizeof(msg->iov) / sizeof(msg->iov[0]); ++i) {
		iobufPtr(msg->iov + i) = NULL;
		iobufLen(msg->iov + i) = 0;
	}
	return msg;
}

SendMsg_t* makeSendMsg(SendMsg_t* msg, int cmdid, const void* data, unsigned int len) {
	msg->htonl_cmdid = htonl(cmdid);
	msg->rpc_status = 0;
	msg->htonl_rpcid = 0;
	msg->identity = 0;
	iobufPtr(msg->iov + 0) = (char*)&msg->rpc_status;
	iobufLen(msg->iov + 0) = sizeof(msg->rpc_status);
	iobufPtr(msg->iov + 1) = (char*)&msg->htonl_cmdid;
	iobufLen(msg->iov + 1) = sizeof(msg->htonl_cmdid);
	iobufPtr(msg->iov + 2) = (char*)&msg->htonl_rpcid;
	iobufLen(msg->iov + 2) = sizeof(msg->htonl_rpcid);
	iobufPtr(msg->iov + 3) = (char*)&msg->identity;
	iobufLen(msg->iov + 3) = sizeof(msg->identity);
	iobufPtr(msg->iov + 4) = len ? (char*)data : NULL;
	iobufLen(msg->iov + 4) = data ? len : 0;
	return msg;
}

SendMsg_t* makeSendMsgRpcReq(SendMsg_t* msg, int rpcid, int cmdid, const void* data, unsigned int len) {
	makeSendMsg(msg, cmdid, data, len);
	msg->rpc_status = RPC_STATUS_REQ;
	msg->htonl_rpcid = htonl(rpcid);
	return msg;
}

SendMsg_t* makeSendMsgRpcResp(SendMsg_t* msg, int rpcid, int retcode, const void* data, unsigned int len) {
	makeSendMsg(msg, retcode, data, len);
	msg->rpc_status = RPC_STATUS_RESP;
	msg->htonl_rpcid = htonl(rpcid);
	return msg;
}

#ifdef __cplusplus
}
#endif