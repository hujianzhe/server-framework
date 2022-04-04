#include "msg_struct.h"
#include "util/inc/sysapi/socket.h"

#ifdef __cplusplus
extern "C" {
#endif

InnerMsg_t* makeInnerMsgEmpty(InnerMsg_t* msg) {
	size_t i;
	for (i = 0; i < sizeof(msg->iov) / sizeof(msg->iov[0]); ++i) {
		iobufPtr(msg->iov + i) = NULL;
		iobufLen(msg->iov + i) = 0;
	}
	return msg;
}

InnerMsg_t* makeInnerMsg(InnerMsg_t* msg, int cmdid, const void* data, unsigned int len) {
	msg->htonl_cmdid = htonl(cmdid);
	msg->rpc_status = 0;
	msg->htonl_rpcid = 0;
	iobufPtr(msg->iov + 0) = (char*)&msg->rpc_status;
	iobufLen(msg->iov + 0) = sizeof(msg->rpc_status);
	iobufPtr(msg->iov + 1) = (char*)&msg->htonl_cmdid;
	iobufLen(msg->iov + 1) = sizeof(msg->htonl_cmdid);
	iobufPtr(msg->iov + 2) = (char*)&msg->htonl_rpcid;
	iobufLen(msg->iov + 2) = sizeof(msg->htonl_rpcid);
	if (data && len) {
		iobufPtr(msg->iov + 3) = (char*)data;
		iobufLen(msg->iov + 3) = len;
	}
	else {
		iobufPtr(msg->iov + 3) = NULL;
		iobufLen(msg->iov + 3) = 0;
	}
	return msg;
}

InnerMsg_t* makeInnerMsgRpcResp(InnerMsg_t* msg, int rpcid, int retcode, const void* data, unsigned int len) {
	makeInnerMsg(msg, retcode, data, len);
	msg->rpc_status = RPC_STATUS_RESP;
	msg->htonl_rpcid = htonl(rpcid);
	return msg;
}

#ifdef __cplusplus
}
#endif
