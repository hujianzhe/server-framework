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
	Channel_t* channel = (Channel_t*)rpc_item->udata;
	if (rpc_item->ret_msg) {
		if (start_req_login_test(channel))
			return;
	}
	else {
		IPString_t ip;
		unsigned short port;
		sockaddrDecode(&channel->_.connect_addr.sa, ip, &port);
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

static int test_timer(RBTimer_t* timer, RBTimerEvent_t* e) {
	logInfo(ptr_g_Log(), "test_timer============================================");
	e->timestamp_msec += e->interval_msec;
	rbtimerAddEvent(timer, e);
	return 0;
}

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(TaskThread_t* thrd, int argc, char** argv) {
	int i;
	RBTimerEvent_t* timer_event;

	regNumberDispatch(thrd->dispatch, CMD_NOTIFY_TEST, notifyTest);
	regNumberDispatch(thrd->dispatch, CMD_RET_TEST, retTest);
	regNumberDispatch(thrd->dispatch, CMD_RET_LOGIN_TEST, retLoginTest);

	// add timer
	timer_event = (RBTimerEvent_t*)malloc(sizeof(RBTimerEvent_t));
	rbtimerEventSet(timer_event, gmtimeMillisecond() / 1000 * 1000 + 1000, test_timer, NULL, 1000);
	rbtimerAddEvent(&thrd->timer, timer_event);

	for (i = 0; i < ptr_g_Config()->connect_options_cnt; ++i) {
		ConfigConnectOption_t* option = ptr_g_Config()->connect_options + i;
		RpcItem_t* rpc_item;
		Sockaddr_t connect_addr;
		Channel_t* c;
		ReactorObject_t* o;
		int domain = ipstrFamily(option->ip);

		if (!sockaddrEncode(&connect_addr.sa, domain, option->ip, option->port))
			return 0;
		o = reactorobjectOpen(INVALID_FD_HANDLE, connect_addr.sa.sa_family, option->socktype, 0);
		if (!o)
			return 0;
		o->stream.max_connect_timeout_sec = 5;
		c = openChannelInner(o, CHANNEL_FLAG_CLIENT, &connect_addr.sa, &thrd->dq);
		if (!c) {
			reactorCommitCmd(NULL, &o->freecmd);
			return 0;
		}
		logInfo(ptr_g_Log(), "channel(%p) connecting......", c);
		if (thrd->f_rpc || thrd->a_rpc) {
			c->_.on_syn_ack = defaultRpcOnSynAck;
			channelUserData(c)->dq = &thrd->dq;
			if (thrd->f_rpc) {
				rpc_item = newRpcItemFiberReady(thrd, c, 5000);
				if (!rpc_item) {
					reactorCommitCmd(NULL, &o->freecmd);
					reactorCommitCmd(NULL, &c->_.freecmd);
					return 1;
				}
				channelUserData(c)->rpc_syn_ack_item = rpc_item;
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
				rpc_item = newRpcItemAsyncReady(thrd, c, 5000, NULL, rpc_async_req_login_test);
				if (!rpc_item) {
					reactorCommitCmd(NULL, &o->freecmd);
					reactorCommitCmd(NULL, &c->_.freecmd);
					return 1;
				}
				rpc_item->udata = (size_t)c;
				channelUserData(c)->rpc_syn_ack_item = rpc_item;
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

__declspec_dllexport void destroy(TaskThread_t* thrd) {

}

#ifdef __cplusplus
}
#endif
