#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

static int start_req_login_test(Channel_t* channel) {
	InnerMsg_t msg;
	makeInnerMsg(&msg, CMD_REQ_LOGIN_TEST, NULL, 0);
	channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	return 1;
}

static void rpc_async_req_login_test(RpcAsyncCore_t* rpc, RpcItem_t* rpc_item) {
	Channel_t* channel = (Channel_t*)rpc_item->originator;
	if (rpc_item->ret_msg) {
		if (start_req_login_test(channel))
			return;
	}
	else {
		IPString_t ip;
		unsigned short port;
		sockaddrDecode(&channel->_.connect_addr.st, ip, &port);
		logErr(ptr_g_Log(), "%s channel(%p) connect %s:%hu failure", __FUNCTION__, channel, ip, port);
	}
	g_Invalid();
}

static void frpc_test_paralle(TaskThread_t* thrd, Channel_t* channel) {
	InnerMsg_t msg;
	char test_data[] = "test paralle ^.^";
	int i, cnt_rpc = 0;
	RpcItem_t* rpc_item;
	for (i = 0; i < 2; ++i) {
		rpc_item = newRpcItemFiberReady(thrd, channel, 1000);
		if (!rpc_item)
			continue;
		makeInnerMsgRpcReq(&msg, rpc_item->id, CMD_REQ_ParallelTest1, test_data, sizeof(test_data));
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		//rpc_item = rpcFiberCoreYield(thrd->f_rpc);
		rpc_item->udata = CMD_REQ_ParallelTest1;
		cnt_rpc++;

		rpc_item = newRpcItemFiberReady(thrd, channel, 1000);
		if (!rpc_item)
			continue;
		makeInnerMsgRpcReq(&msg, rpc_item->id, CMD_REQ_ParallelTest2, test_data, sizeof(test_data));
		channelSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		//rpc_item = rpcFiberCoreYield(thrd->f_rpc);
		rpc_item->udata = CMD_REQ_ParallelTest2;
		cnt_rpc++;
	}
	while (cnt_rpc--) {
		UserMsg_t* ret_msg;
		rpc_item = rpcFiberCoreYield(thrd->f_rpc);
		if (!rpc_item->ret_msg) {
			printf("rpc identity(%zu) call failure timeout or cancel\n", rpc_item->udata);
			continue;
		}
		ret_msg = (UserMsg_t*)rpc_item->ret_msg;
		printf("rpc identity(%zu) return: %s ...\n", rpc_item->udata, ret_msg->data);
	}
}

static void websocket_recv(Channel_t* c, const void* addr, ChannelInbufDecodeResult_t* decode_result) {
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

static int test_timer(RBTimer_t* timer, RBTimerEvent_t* e) {
	logInfo(ptr_g_Log(), "test_timer============================================");
	//e->timestamp_msec += 1000;
	//rbtimerAddEvent(timer, e);
	return 0;
}

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(TaskThread_t* thrd, int argc, char** argv) {
	int i;
	RBTimerEvent_t* timer_event;

	regNumberDispatch(thrd->dispatch, CMD_REQ_TEST, reqTest);
	regNumberDispatch(thrd->dispatch, CMD_NOTIFY_TEST, notifyTest);
	regNumberDispatch(thrd->dispatch, CMD_RET_TEST, retTest);
	regNumberDispatch(thrd->dispatch, CMD_REQ_LOGIN_TEST, reqLoginTest);
	regNumberDispatch(thrd->dispatch, CMD_RET_LOGIN_TEST, retLoginTest);
	regStringDispatch(thrd->dispatch, "/reqHttpTest", reqHttpTest);
	regStringDispatch(thrd->dispatch, "/reqSoTest", reqSoTest);
	regStringDispatch(thrd->dispatch, "/reqHttpUploadFile", reqHttpUploadFile);
	regNumberDispatch(thrd->dispatch, CMD_REQ_WEBSOCKET_TEST, reqWebsocketTest);
	regNumberDispatch(thrd->dispatch, CMD_REQ_ParallelTest1, reqParallelTest1);
	regNumberDispatch(thrd->dispatch, CMD_REQ_ParallelTest2, reqParallelTest2);

	// add timer
	timer_event = (RBTimerEvent_t*)malloc(sizeof(RBTimerEvent_t));
	timer_event->arg = NULL;
	timer_event->callback = test_timer;
	timer_event->timestamp_msec = gmtimeMillisecond() + 1000;
	rbtimerAddEvent(&thrd->timer, timer_event);

	// listen extra port
	for (i = 0; i < ptr_g_Config()->listen_options_cnt; ++i) {
		ConfigListenOption_t* option = ptr_g_Config()->listen_options + i;
		Channel_t* c;
		if (!strcmp(option->protocol, "http")) {
			c = openListenerHttp(option->ip, option->port, NULL);
		}
		else if (!strcmp(option->protocol, "websocket")) {
			c = openListenerWebsocket(option->ip, option->port, websocket_recv);
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

	for (i = 0; i < ptr_g_Config()->connect_options_cnt; ++i) {
		ConfigConnectOption_t* option = ptr_g_Config()->connect_options + i;
		RpcItem_t* rpc_item;
		Sockaddr_t connect_addr;
		Channel_t* c;
		ReactorObject_t* o;
		int domain = ipstrFamily(option->ip);

		if (!sockaddrEncode(&connect_addr.st, domain, option->ip, option->port))
			return 0;
		o = reactorobjectOpen(INVALID_FD_HANDLE, connect_addr.st.ss_family, option->socktype, 0);
		if (!o)
			return 0;
		c = openChannelInner(o, CHANNEL_FLAG_CLIENT, &connect_addr);
		if (!c) {
			reactorCommitCmd(NULL, &o->freecmd);
			return 0;
		}
		logInfo(ptr_g_Log(), "channel(%p) connecting......", c);
		if (thrd->f_rpc || thrd->a_rpc) {
			c->_.on_syn_ack = defaultRpcOnSynAck;
			if (thrd->f_rpc) {
				if (!newRpcItemFiberReady(thrd, c, 5000)) {
					reactorCommitCmd(NULL, &o->freecmd);
					reactorCommitCmd(NULL, &c->_.freecmd);
					return 1;
				}
				reactorCommitCmd(selectReactor(), &o->regcmd);
				rpc_item = rpcFiberCoreYield(thrd->f_rpc);
				if (rpc_item->ret_msg) {
					frpc_test_paralle(thrd, c);
					if (!start_req_login_test(c))
						return 0;
				}
				else {
					logErr(ptr_g_Log(), "channel(%p) connect %s:%u failure", c, option->ip, option->port);
					return 0;
				}
			}
			else {
				if (!newRpcItemAsyncReady(thrd, c, 5000, NULL, rpc_async_req_login_test)) {
					reactorCommitCmd(NULL, &o->freecmd);
					reactorCommitCmd(NULL, &c->_.freecmd);
					return 1;
				}
				reactorCommitCmd(selectReactor(), &o->regcmd);
			}
		}
		else {
			reactorCommitCmd(selectReactor(), &o->regcmd);
			if (!start_req_login_test(c))
				return 0;
		}
	}

	return 1;
}

__declspec_dllexport void destroy(void) {

}

#ifdef __cplusplus
}
#endif
