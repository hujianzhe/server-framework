#ifndef BOOT_SERVER_DISPATCH_MSG_H
#define BOOT_SERVER_DISPATCH_MSG_H

#include "util/inc/component/reactor.h"

struct SerialExecObj_t;
struct TaskThread_t;
struct UserMsg_t;
typedef void(*DispatchCallback_t)(struct TaskThread_t*, struct UserMsg_t*);

typedef struct SerialExecQueue_t {
	List_t list;
	struct SerialExecObj_t* exec_obj;
} SerialExecQueue_t;

typedef struct SerialExecObj_t {
	ListNode_t listnode;
	SerialExecQueue_t* dq;
	short hang_up;
} SerialExecObj_t;

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

SerialExecObj_t* SerialExecQueue_next(SerialExecQueue_t* dq);

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll UserMsg_t* newUserMsg(size_t datalen);
__declspec_dll void freeUserMsg(UserMsg_t* msg);
__declspec_dll void freeUserMsgSerial(SerialExecObj_t* serial);

__declspec_dll SerialExecQueue_t* SerialExecQueue_init(SerialExecQueue_t* dq);
__declspec_dll int SerialExecQueue_check_exec(SerialExecQueue_t* dq, SerialExecObj_t* obj);
__declspec_dll void SerialExecQueue_clear(SerialExecQueue_t* dq, void(*fn_free)(SerialExecObj_t*));

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_MSG_H
