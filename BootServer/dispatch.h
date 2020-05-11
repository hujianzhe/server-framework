#ifndef DISPATCH_H
#define	DISPATCH_H

#include "util/inc/component/channel.h"
#include "util/inc/component/httpframe.h"

typedef struct UserMsg_t {
	ReactorCmd_t internal;
	Channel_t* channel;
	Sockaddr_t peer_addr;
	HttpFrame_t* httpframe;
	char rpc_status;
	int cmdid;
	int rpcid;
	size_t datalen;
	unsigned char data[1];
} UserMsg_t;

typedef void(*DispatchCallback_t)(UserMsg_t*);

#ifdef __cplusplus
extern "C" {
#endif

int initDispatch(void);
__declspec_dll int regStringDispatch(const char* str, DispatchCallback_t func);
__declspec_dll int regNumberDispatch(int cmd, DispatchCallback_t func);
DispatchCallback_t getStringDispatch(const char* str);
DispatchCallback_t getNumberDispatch(int cmd);
void freeDispatchCallback(void);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_H
