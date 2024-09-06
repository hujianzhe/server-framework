#ifndef BOOT_SERVER_DISPATCH_MSG_H
#define BOOT_SERVER_DISPATCH_MSG_H

#include "util/inc/component/net_reactor.h"

typedef struct DispatchBaseMsg_t {
	int rpcid;
	void(*on_free)(struct DispatchBaseMsg_t* self);
} DispatchBaseMsg_t;

struct TaskThread_t;
struct DispatchNetMsg_t;
typedef void(*DispatchNetCallback_t)(struct TaskThread_t*, struct DispatchNetMsg_t*);

typedef struct DispatchNetMsg_t {
	DispatchBaseMsg_t base;
	struct {
		short type;
		const void* value; /* any value */
	} param;
	long long enqueue_time_msec;
	DispatchNetCallback_t callback;
	NetChannel_t* channel;
	Sockaddr_t peer_addr;
	socklen_t peer_addrlen;
	int retcode;
	size_t datalen;
	unsigned char data[1];
} DispatchNetMsg_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll DispatchNetMsg_t* newDispatchNetMsg(NetChannel_t* channel, size_t datalen, void(*on_free)(DispatchBaseMsg_t*));
__declspec_dll void freeDispatchNetMsg(DispatchBaseMsg_t* msg);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_MSG_H
