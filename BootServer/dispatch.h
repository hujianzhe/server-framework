#ifndef BOOT_SERVER_DISPATCH_H
#define	BOOT_SERVER_DISPATCH_H

#include "dispatch_msg.h"

struct Dispatch_t;

struct Dispatch_t* newDispatch(void);
void freeDispatch(struct Dispatch_t* dispatch);

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll DispatchCallback_t regNullDispatch(struct Dispatch_t* dispatch, DispatchCallback_t func);
__declspec_dll int regStringDispatch(struct Dispatch_t* dispatch, const char* str, DispatchCallback_t func);
__declspec_dll int regNumberDispatch(struct Dispatch_t* dispatch, int cmd, DispatchCallback_t func);
__declspec_dll DispatchCallback_t getStringDispatch(const struct Dispatch_t* dispatch, const char* str);
__declspec_dll DispatchCallback_t getNumberDispatch(const struct Dispatch_t* dispatch, int cmd);

#ifdef __cplusplus
}
#endif

#endif // !DISPATCH_H
