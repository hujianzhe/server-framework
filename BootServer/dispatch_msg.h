#ifndef BOOT_SERVER_DISPATCH_MSG_H
#define BOOT_SERVER_DISPATCH_MSG_H

#include "util/inc/component/reactor.h"

struct TaskThread_t;
struct UserMsg_t;
typedef void(*DispatchCallback_t)(struct TaskThread_t*, struct UserMsg_t*);

typedef struct UserMsg_t {
	SerialExecObj_t serial;
	ChannelBase_t* channel;
	Sockaddr_t peer_addr;
	void(*on_free)(struct UserMsg_t* self);
	struct {
		short type;
		const void* value; /* any value */
	} param;
	long long enqueue_time_msec;
	DispatchCallback_t callback;
	int retcode;
	int rpcid;
	size_t datalen;
	unsigned char data[1];
} UserMsg_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll UserMsg_t* newUserMsg(size_t datalen);
__declspec_dll void freeUserMsg(UserMsg_t* msg);
__declspec_dll void freeUserMsgSerial(SerialExecObj_t* serial);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_MSG_H
