#ifndef	BOOT_SERVER_NET_CHANNEL_PROC_IMP_H
#define	BOOT_SERVER_NET_CHANNEL_PROC_IMP_H

#include "util/inc/component/net_reactor.h"
#include "util/inc/component/net_channel_ex.h"

struct DispatchNetMsg_t;

typedef struct NetChannelUserData_t {
	void* sche;
	int rpc_id_syn_ack;
	int text_data_print_log;
} NetChannelUserData_t;

typedef struct NetScheHook_t {
	void(*on_detach)(void* sche, NetChannel_t* channel);
	void(*on_execute_msg)(void* sche, struct DispatchNetMsg_t* msg);
	void(*on_resume_msg)(void* sche, struct DispatchNetMsg_t* msg);
	void(*on_resume)(void* sche, int id, int canceled);
} NetScheHook_t;

typedef void(*FnChannelOnRecv_t)(NetChannel_t*, unsigned char*, size_t, const struct sockaddr*, socklen_t);

#define	NetChannel_get_userdata(channel)		((NetChannelUserData_t*)((channel)->userdata))
#define	NetChannel_set_userdata(channel, ud)	((channel)->userdata = (ud))

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll const NetScheHook_t* getNetScheHookStackCo(void);

__declspec_dll NetChannelUserData_t* initChannelUserData(NetChannelUserData_t* ud, void* sche);
__declspec_dll void defaultRpcOnSynAck(NetChannel_t* c, long long ts_msec);
__declspec_dll void defaultChannelOnDetach(NetChannel_t* c);

#ifdef __cplusplus
}
#endif

#endif