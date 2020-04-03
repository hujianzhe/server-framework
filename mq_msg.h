#ifndef MQ_MSG_H
#define	MQ_MSG_H

#include "mq_socket.h"

typedef struct MQRecvMsg_t {
	ReactorCmd_t internal;
	Channel_t* channel;
	Sockaddr_t peer_addr;
	int cmd;
	size_t datalen;
	unsigned char data[1];
} MQRecvMsg_t;

typedef struct MQSendMsg_t {
	int htonl_cmd;
	Iobuf_t iov[2];
} MQSendMsg_t;

MQSendMsg_t* makeMQSendMsg(MQSendMsg_t* msg, int cmd, const void* data, unsigned int len);
MQSendMsg_t* makeMQSendMsgEmpty(MQSendMsg_t* msg);

#endif // !MQ_MSG_H
