#ifndef BOOT_SERVER_DISPATCH_H
#define	BOOT_SERVER_DISPATCH_H

#include "util/inc/component/channel.h"

enum {
	USER_MSG_PARAM_INIT = 1,
	USER_MSG_PARAM_HTTP_FRAME,
	USER_MSG_PARAM_TIMER_EVENT,
	USER_MSG_PARAM_CHANNEL_DETACH,
};

struct HttpFrame_t;
struct RBTimerEvent_t;

typedef struct UserMsg_t {
	ReactorCmd_t internal;
	ChannelBase_t* channel;
	Sockaddr_t peer_addr;
	void(*on_free)(struct UserMsg_t* self);
	short be_from_cluster;
	struct {
		short type;
		union {
			struct HttpFrame_t* httpframe;
			struct RBTimerEvent_t* timer_event; /* fiber use */
			const void* value; /* any value */
		};
	} param;
	long long enqueue_time_msec;
	const char* cmdstr;
	char rpc_status;
	union {
		int cmdid;
		int retcode;
	};
	int rpcid;
	size_t datalen;
	unsigned char data[1];
} UserMsg_t;

struct TaskThread_t;
typedef void(*DispatchCallback_t)(struct TaskThread_t*, UserMsg_t*);

typedef struct Dispatch_t {
	DispatchCallback_t null_dispatch_callback;
	Hashtable_t s_NumberDispatchTable;
	HashtableNode_t* s_NumberDispatchBulk[1024];
	Hashtable_t s_StringDispatchTable;
	HashtableNode_t* s_StringDispatchBulk[1024];
} Dispatch_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll UserMsg_t* newUserMsg(size_t datalen);

Dispatch_t* newDispatch(void);
__declspec_dll int regStringDispatch(Dispatch_t* dispatch, const char* str, DispatchCallback_t func);
__declspec_dll int regNumberDispatch(Dispatch_t* dispatch, int cmd, DispatchCallback_t func);
DispatchCallback_t getStringDispatch(const Dispatch_t* dispatch, const char* str);
DispatchCallback_t getNumberDispatch(const Dispatch_t* dispatch, int cmd);
void freeDispatch(Dispatch_t* dispatch);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_H
