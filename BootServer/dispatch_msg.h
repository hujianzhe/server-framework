#ifndef BOOT_SERVER_DISPATCH_MSG_H
#define BOOT_SERVER_DISPATCH_MSG_H

#include "util/inc/component/reactor.h"

struct TaskThread_t;
struct UserMsg_t;
typedef void(*DispatchCallback_t)(struct TaskThread_t*, struct UserMsg_t*);

typedef struct UserMsgExecQueue_t {
	List_t list;
	struct UserMsg_t* exec_msg;
} UserMsgExecQueue_t;

typedef struct UserMsg_t {
	ListNode_t order_listnode;
	UserMsgExecQueue_t* order_dq;
	ChannelBase_t* channel;
	Sockaddr_t peer_addr;
	void(*on_free)(struct UserMsg_t* self);
	short hang_up;
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

UserMsg_t* UserMsgExecQueue_next(UserMsgExecQueue_t* dq);

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll UserMsg_t* newUserMsg(size_t datalen);
__declspec_dll void freeUserMsg(UserMsg_t* msg);

__declspec_dll UserMsgExecQueue_t* UserMsgExecQueue_init(UserMsgExecQueue_t* dq);
__declspec_dll int UserMsgExecQueue_check_exec(UserMsgExecQueue_t* dq, UserMsg_t* msg);
__declspec_dll void UserMsgExecQueue_clear(UserMsgExecQueue_t* dq);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_MSG_H
