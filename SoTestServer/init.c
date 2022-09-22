#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

static void websocket_recv(ChannelBase_t* c, const struct sockaddr* addr, const ChannelInbufDecodeResult_t* decode_result) {
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
		if (!(c->flag & CHANNEL_FLAG_STREAM)) {
			memcpy(&message->peer_addr, addr, sockaddrLength(addr));
		}
		message->rpc_status = 0;
		message->cmdid = cmdid;
		message->rpcid = 0;
		if (message->datalen) {
			memcpy(message->data, decode_result->bodyptr, message->datalen);
		}
		if (ptrBSG()->conf->enqueue_timeout_msec > 0) {
			message->enqueue_time_msec = gmtimeMillisecond();
		}
		dataqueuePush(channelUserData(c)->dq, &message->internal._);
	}
	else if (c->flag & CHANNEL_FLAG_SERVER) {
		channelbaseSend(c, NULL, 0, NETPACKET_NO_ACK_FRAGMENT);
	}
}

int init(TaskThread_t* thrd, int argc, char** argv) {
	int i;
	// register dispatch
	regNumberDispatch(thrd->dispatch, CMD_REQ_TEST, reqTest);
	regNumberDispatch(thrd->dispatch, CMD_REQ_TEST_CALLBACK, reqTestCallback);
	regNumberDispatch(thrd->dispatch, CMD_REQ_LOGIN_TEST, reqLoginTest);
	regStringDispatch(thrd->dispatch, "/reqHttpTest", reqHttpTest);
	regStringDispatch(thrd->dispatch, "/reqSoTest", reqSoTest);
	regStringDispatch(thrd->dispatch, "/reqHttpUploadFile", reqHttpUploadFile);
	regNumberDispatch(thrd->dispatch, CMD_REQ_WEBSOCKET_TEST, reqWebsocketTest);
	regNumberDispatch(thrd->dispatch, CMD_REQ_ParallelTest1, reqParallelTest1);
	regNumberDispatch(thrd->dispatch, CMD_REQ_ParallelTest2, reqParallelTest2);
	// listen extra port
	for (i = 0; i < ptrBSG()->conf->listen_options_cnt; ++i) {
		ConfigListenOption_t* option = ptrBSG()->conf->listen_options + i;
		ChannelBase_t* c;
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
			logErr(ptrBSG()->log, "listen failure, ip:%s, port:%u ......", option->ip, option->port);
			return 0;
		}
		reactorCommitCmd(acceptReactor(), &c->o->regcmd);
	}

	return 1;
}

void destroy(TaskThread_t* thrd) {

}
