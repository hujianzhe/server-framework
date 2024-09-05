#ifndef BOOT_SERVER_NET_CHANNEL_INNER_H
#define	BOOT_SERVER_NET_CHANNEL_INNER_H

#include "net_channel_proc_imp.h"
#include "util/inc/sysapi/io_overlapped.h"

typedef struct InnerMsgPayload_t {
	char rpc_status;
	union {
		int htonl_cmdid;
		int htonl_retcode;
	};
	int htonl_rpcid;
	Iobuf_t iov[4];
} InnerMsgPayload_t;

enum {
	INNER_MSG_STRUCT_IOV_CNT = 4
};

enum {
	INNER_MSG_FIELD_RPC_STATUS_REQ = 'R',
	INNER_MSG_FIELD_RPC_STATUS_RESP = 'T',
};

enum {
	INNER_MSG_FORMAT_BASEHDRSIZE = 4,
	INNER_MSG_FORMAT_EXTHDRSIZE = 6,
	INNER_MSG_FORMAT_HDRSIZE = INNER_MSG_FORMAT_BASEHDRSIZE + INNER_MSG_FORMAT_EXTHDRSIZE
};

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll NetChannel_t* openNetChannelInnerClient(int socktype, const char* ip, unsigned short port, void* sche);
__declspec_dll NetChannel_t* openNetListenerInner(int socktype, const char* ip, unsigned short port, void* sche);

__declspec_dll InnerMsgPayload_t* makeInnerMsgEmpty(InnerMsgPayload_t* msg);
__declspec_dll InnerMsgPayload_t* makeInnerMsg(InnerMsgPayload_t* msg, int cmdid, const void* data, unsigned int len);
__declspec_dll InnerMsgPayload_t* makeInnerMsgRpcReq(InnerMsgPayload_t* msg, int rpcid, int cmdid, const void* data, unsigned int len);
__declspec_dll InnerMsgPayload_t* makeInnerMsgRpcResp(InnerMsgPayload_t* msg, int rpcid, int retcode, const void* data, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif