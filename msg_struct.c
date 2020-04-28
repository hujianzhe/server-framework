#include "msg_struct.h"

SendMsg_t* makeSendMsg(SendMsg_t* msg, int cmdid, const void* data, unsigned int len) {
	msg->htonl_cmdid = htonl(cmdid);
	msg->rpc_status = 0;
	msg->htonl_rpcid = 0;
	iobufPtr(msg->iov + 0) = (char*)&msg->rpc_status;
	iobufLen(msg->iov + 0) = sizeof(msg->rpc_status);
	iobufPtr(msg->iov + 1) = (char*)&msg->htonl_cmdid;
	iobufLen(msg->iov + 1) = sizeof(msg->htonl_cmdid);
	iobufPtr(msg->iov + 2) = (char*)&msg->htonl_rpcid;
	iobufLen(msg->iov + 2) = sizeof(msg->htonl_rpcid);
	iobufPtr(msg->iov + 3) = len ? (char*)data : NULL;
	iobufLen(msg->iov + 3) = data ? len : 0;
	return msg;
}

SendMsg_t* makeSendMsgRpcReq(SendMsg_t* msg, int cmdid, int rpcid, const void* data, unsigned int len) {
	makeSendMsg(msg, cmdid, data, len);
	msg->rpc_status = 'R';
	msg->htonl_rpcid = htonl(rpcid);
	return msg;
}

SendMsg_t* makeSendMsgRpcResp(SendMsg_t* msg, int rpcid, const void* data, unsigned int len) {
	makeSendMsg(msg, 0, data, len);
	msg->rpc_status = 'T';
	msg->htonl_rpcid = htonl(rpcid);
	return msg;
}

SendMsg_t* makeSendMsgEmpty(SendMsg_t* msg) {
	int i;
	for (i = 0; i < sizeof(msg->iov) / sizeof(msg->iov[0]); ++i) {
		iobufPtr(msg->iov + i) = NULL;
		iobufLen(msg->iov + i) = 0;
	}
	return msg;
}
