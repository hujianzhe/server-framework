#ifndef BOOT_SERVER_DISPATCH_H
#define	BOOT_SERVER_DISPATCH_H

#include "util/inc/component/reactor.h"

struct TaskThread_t;
struct UserMsg_t;
typedef void(*DispatchCallback_t)(struct TaskThread_t*, struct UserMsg_t*);

typedef struct UserMsg_t {
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

struct Dispatch_t;

struct Dispatch_t* newDispatch(void);
void freeDispatch(struct Dispatch_t* dispatch);

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll UserMsg_t* newUserMsg(size_t datalen);

__declspec_dll DispatchCallback_t regNullDispatch(struct Dispatch_t* dispatch, DispatchCallback_t func);
__declspec_dll int regStringDispatch(struct Dispatch_t* dispatch, const char* str, DispatchCallback_t func);
__declspec_dll int regNumberDispatch(struct Dispatch_t* dispatch, int cmd, DispatchCallback_t func);
__declspec_dll DispatchCallback_t getStringDispatch(const struct Dispatch_t* dispatch, const char* str);
__declspec_dll DispatchCallback_t getNumberDispatch(const struct Dispatch_t* dispatch, int cmd);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_H
