#include "util/inc/component/lengthfieldframe.h"
#include "mq_msg.h"

MQSendMsg_t* makeMQSendMsg(MQSendMsg_t* msg, int cmd, const void* data, unsigned int len) {
	msg->htonl_cmd = htonl(cmd);
	iobufPtr(msg->iov + 0) = (char*)&msg->htonl_cmd;
	iobufLen(msg->iov + 0) = sizeof(cmd);
	iobufPtr(msg->iov + 1) = len ? (char*)data : NULL;
	iobufLen(msg->iov + 1) = data ? len : 0;
	return msg;
}

MQSendMsg_t* makeMQSendMsgEmpty(MQSendMsg_t* msg) {
	iobufPtr(msg->iov + 0) = NULL;
	iobufLen(msg->iov + 0) = 0;
	iobufPtr(msg->iov + 1) = NULL;
	iobufLen(msg->iov + 1) = 0;
	return msg;
}
