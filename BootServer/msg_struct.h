#ifndef BOOT_SERVER_MSG_STRUCT_H
#define	BOOT_SERVER_MSG_STRUCT_H

#include "util/inc/platform_define.h"

typedef struct InnerMsg_t {
	char rpc_status;
	union {
		int htonl_cmdid;
		int htonl_retcode;
	};
	int htonl_rpcid;
	Iobuf_t iov[4];
} InnerMsg_t;

enum {
	INNER_MSG_HEAD_PART_IOV_CNT = 3,
	INNER_MSG_IOV_CNT = 4
};

enum {
	RPC_STATUS_REQ = 'R',
	RPC_STATUS_RESP = 'T',
	RPC_STATUS_HAND_SHAKE = 'S',
};

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dll InnerMsg_t* makeInnerMsgEmpty(InnerMsg_t* msg);
__declspec_dll InnerMsg_t* makeInnerMsg(InnerMsg_t* msg, int cmdid, const void* data, unsigned int len);

__declspec_dll InnerMsg_t* makeInnerMsgRpcReq(InnerMsg_t* msg, int rpcid, int cmdid, const void* data, unsigned int len);
__declspec_dll InnerMsg_t* makeInnerMsgRpcResp(InnerMsg_t* msg, int rpcid, int retcode, const void* data, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif // !MSG_STRUCT_H
