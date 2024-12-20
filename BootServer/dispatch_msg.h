#ifndef BOOT_SERVER_DISPATCH_MSG_H
#define BOOT_SERVER_DISPATCH_MSG_H

#include "util/inc/component/net_reactor.h"
#include <stdint.h>

struct TaskThread_t;
struct DispatchNetMsg_t;
typedef void(*DispatchNetCallback_t)(struct TaskThread_t*, struct DispatchNetMsg_t*);

typedef struct DispatchNetMsg_t {
	int64_t rpcid;
	void(*on_free)(struct DispatchNetMsg_t* self);
	struct {
		short type;
		const void* value; /* any value */
	} param;
	long long enqueue_time_msec;
	DispatchNetCallback_t callback;
	NetChannel_t* channel;
	struct sockaddr* peer_addr;
	socklen_t peer_addrlen;
	int retcode;
	int cmd;
	size_t datalen;
	unsigned char data[1];
} DispatchNetMsg_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll DispatchNetMsg_t* newDispatchNetMsg(size_t datalen, socklen_t saddrlen);
__declspec_dll void freeDispatchNetMsg(DispatchNetMsg_t* msg);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_MSG_H
