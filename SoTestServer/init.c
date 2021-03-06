#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

static void websocket_recv(Channel_t* c, const struct sockaddr* addr, ChannelInbufDecodeResult_t* decode_result) {
	if (decode_result->bodylen > 0) {
		UserMsg_t* message;
		char* cmdstr;
		int cmdid;

		cmdstr = strstr((char*)decode_result->bodyptr, "cmd");
		if (!cmdstr) {
			return;
		}
		cmdstr += 3;
		cmdstr = strchr(cmdstr, ':');
		if (!cmdstr) {
			return;
		}
		cmdstr++;
		if (sscanf(cmdstr, "%d", &cmdid) != 1) {
			return;
		}

		message = newUserMsg(decode_result->bodylen);
		if (!message) {
			return;
		}
		message->channel = c;
		if (!(c->_.flag & CHANNEL_FLAG_STREAM)) {
			memcpy(&message->peer_addr, addr, sockaddrLength(addr));
		}
		message->rpc_status = 0;
		message->cmdid = cmdid;
		message->rpcid = 0;
		if (message->datalen) {
			memcpy(message->data, decode_result->bodyptr, message->datalen);
		}
		if (ptr_g_Config()->enqueue_timeout_msec > 0) {
			message->enqueue_time_msec = gmtimeMillisecond();
		}
		dataqueuePush(&ptr_g_TaskThread()->dq, &message->internal._);
	}
	else if (c->_.flag & CHANNEL_FLAG_SERVER) {
		channelSend(c, NULL, 0, NETPACKET_NO_ACK_FRAGMENT);
	}
}

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(TaskThread_t* thrd, int argc, char** argv) {
	int i;
	// register dispatch
	regNumberDispatch(thrd->dispatch, CMD_REQ_TEST, reqTest);
	regNumberDispatch(thrd->dispatch, CMD_REQ_LOGIN_TEST, reqLoginTest);
	regStringDispatch(thrd->dispatch, "/reqHttpTest", reqHttpTest);
	regStringDispatch(thrd->dispatch, "/reqSoTest", reqSoTest);
	regStringDispatch(thrd->dispatch, "/reqHttpUploadFile", reqHttpUploadFile);
	regNumberDispatch(thrd->dispatch, CMD_REQ_WEBSOCKET_TEST, reqWebsocketTest);
	regNumberDispatch(thrd->dispatch, CMD_REQ_ParallelTest1, reqParallelTest1);
	regNumberDispatch(thrd->dispatch, CMD_REQ_ParallelTest2, reqParallelTest2);
	// listen extra port
	for (i = 0; i < ptr_g_Config()->listen_options_cnt; ++i) {
		ConfigListenOption_t* option = ptr_g_Config()->listen_options + i;
		Channel_t* c;
		if (!strcmp(option->protocol, "http")) {
			c = openListenerHttp(option->ip, option->port, NULL, &thrd->dq);
		}
		else if (!strcmp(option->protocol, "websocket")) {
			c = openListenerWebsocket(option->ip, option->port, websocket_recv, &thrd->dq);
		}
		else {
			continue;
		}
		if (!c) {
			logErr(ptr_g_Log(), "listen failure, ip:%s, port:%u ......", option->ip, option->port);
			return 0;
		}
		reactorCommitCmd(ptr_g_ReactorAccept(), &c->_.o->regcmd);
	}

	return 1;
}

__declspec_dllexport void destroy(TaskThread_t* thrd) {

}

#ifdef __cplusplus
}
#endif
