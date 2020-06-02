#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

static int start_req_login_test(Channel_t* channel) {
	SendMsg_t msg;
	makeSendMsg(&msg, CMD_REQ_LOGIN_TEST, NULL, 0);
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

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(TaskThread_t* thrd, int argc, char** argv) {
	int connectsockinitokcnt;

	regNumberDispatch(CMD_REQ_TEST, reqTest);
	regNumberDispatch(CMD_NOTIFY_TEST, notifyTest);
	regNumberDispatch(CMD_RET_TEST, retTest);
	regNumberDispatch(CMD_REQ_LOGIN_TEST, reqLoginTest);
	regNumberDispatch(CMD_RET_LOGIN_TEST, retLoginTest);
	regStringDispatch("/reqHttpTest", reqHttpTest);
	regStringDispatch("/reqSoTest", reqSoTest);
	regNumberDispatch(CMD_REQ_WEBSOCKET_TEST, reqWebsocketTest);

	if (getClusterSelf()->port) {
		ReactorObject_t* o = openListener(getClusterSelf()->socktype, getClusterSelf()->ip, getClusterSelf()->port);
		if (!o)
			return 0;
		reactorCommitCmd(ptr_g_ReactorAccept(), &o->regcmd);
	}

	for (connectsockinitokcnt = 0; connectsockinitokcnt < ptr_g_Config()->connect_options_cnt; ++connectsockinitokcnt) {
		ConfigConnectOption_t* option = ptr_g_Config()->connect_options + connectsockinitokcnt;
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
		c = openChannel(o, CHANNEL_FLAG_CLIENT, &connect_addr);
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
				reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);
				rpc_item = rpcFiberCoreYield(thrd->f_rpc);
				if (rpc_item->ret_msg) {
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
				reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);
			}
		}
		else {
			reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);
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