#ifndef DISPATCH_H
#define	DISPATCH_H

#include "util/inc/component/channel.h"
#include "util/inc/component/httpframe.h"

typedef struct UserMsg_t {
	ReactorCmd_t internal;
	Channel_t* channel;
	Sockaddr_t peer_addr;
	int be_from_cluster;
	HttpFrame_t* httpframe;
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

typedef struct Dispatch_t {
	Hashtable_t s_NumberDispatchTable;
	HashtableNode_t* s_NumberDispatchBulk[1024];
	Hashtable_t s_StringDispatchTable;
	HashtableNode_t* s_StringDispatchBulk[1024];
} Dispatch_t;

typedef void(*DispatchCallback_t)(struct TaskThread_t*, UserMsg_t*);

extern DispatchCallback_t g_DefaultDispatchCallback;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport UserMsg_t* newUserMsg(size_t datalen);

__declspec_dllexport void set_g_DefaultDispatchCallback(DispatchCallback_t fn);

Dispatch_t* newDispatch(void);
__declspec_dllexport int regStringDispatch(Dispatch_t* dispatch, const char* str, DispatchCallback_t func);
__declspec_dllexport int regNumberDispatch(Dispatch_t* dispatch, int cmd, DispatchCallback_t func);
DispatchCallback_t getStringDispatch(Dispatch_t* dispatch, const char* str);
DispatchCallback_t getNumberDispatch(Dispatch_t* dispatch, int cmd);
void freeDispatch(Dispatch_t* dispatch);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_H
