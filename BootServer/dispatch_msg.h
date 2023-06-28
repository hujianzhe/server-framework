#ifndef BOOT_SERVER_DISPATCH_MSG_H
#define BOOT_SERVER_DISPATCH_MSG_H

#include "util/inc/component/reactor.h"

typedef struct DispatchBaseMsg_t {
	SerialExecObj_t serial;
	int rpcid;
	int dispatch_net_msg_type;
	void(*on_free)(struct DispatchBaseMsg_t* self);
} DispatchBaseMsg_t;

struct TaskThread_t;
struct DispatchNetMsg_t;
typedef void(*DispatchCallback_t)(struct TaskThread_t*, struct DispatchNetMsg_t*);

typedef struct DispatchNetMsg_t {
	DispatchBaseMsg_t base;
	struct {
		short type;
		const void* value; /* any value */
	} param;
	long long enqueue_time_msec;
	DispatchCallback_t callback;
	ChannelBase_t* channel;
	Sockaddr_t peer_addr;
	socklen_t peer_addrlen;
	int retcode;
	size_t datalen;
	unsigned char data[1];
} DispatchNetMsg_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll DispatchNetMsg_t* newDispatchNetMsg(ChannelBase_t* channel, size_t datalen, void(*on_free)(DispatchBaseMsg_t*));
__declspec_dll void freeDispatchNetMsg(DispatchBaseMsg_t* msg);
__declspec_dll void freeDispatchMsgSerial(SerialExecObj_t* serial);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_MSG_H
