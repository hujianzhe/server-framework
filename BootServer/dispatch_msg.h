#ifndef BOOT_SERVER_DISPATCH_MSG_H
#define BOOT_SERVER_DISPATCH_MSG_H

#include "util/inc/component/reactor.h"

struct TaskThread_t;
struct UserMsg_t;
typedef void(*DispatchCallback_t)(struct TaskThread_t*, struct UserMsg_t*);

typedef struct UserMsg_t {
	ListNode_t order_listnode;
	ChannelBase_t* channel;
	Sockaddr_t peer_addr;
	void(*on_free)(struct UserMsg_t* self);
	short retry;
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

typedef struct UserMsgExecQueue_t {
	List_t list;
	short waiting;
	short removed;
} UserMsgExecQueue_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll UserMsg_t* newUserMsg(size_t datalen);

__declspec_dll UserMsgExecQueue_t* UserMsgExecQueue_init(UserMsgExecQueue_t* mq);
__declspec_dll int UserMsgExecQueue_try_exec(UserMsgExecQueue_t* mq, struct TaskThread_t* thrd, UserMsg_t* msg);
__declspec_dll void UserMsgExecQueue_clear(UserMsgExecQueue_t* mq);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_MSG_H
