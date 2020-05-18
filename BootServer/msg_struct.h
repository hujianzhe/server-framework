#ifndef MSG_STRUCT_H
#define	MSG_STRUCT_H

#include "util/inc/platform_define.h"

typedef struct SendMsg_t {
	char rpc_status;
	union {
		int htonl_cmdid;
		int htonl_retcode;
	};
	int htonl_rpcid;
	Iobuf_t iov[4];
} SendMsg_t;

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport SendMsg_t* makeSendMsgEmpty(SendMsg_t* msg);
__declspec_dllexport SendMsg_t* makeSendMsg(SendMsg_t* msg, int cmdid, const void* data, unsigned int len);
__declspec_dllexport SendMsg_t* makeSendMsgRpcReq(SendMsg_t* msg, int rpcid, int cmdid, const void* data, unsigned int len);
__declspec_dllexport SendMsg_t* makeSendMsgRpcResp(SendMsg_t* msg, int rpcid, int retcode, const void* data, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif // !MSG_STRUCT_H
