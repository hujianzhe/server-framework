#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

static int start_req_login_test(ChannelBase_t* channel) {
	InnerMsg_t msg;
	makeInnerMsg(&msg, CMD_REQ_LOGIN_TEST, NULL, 0);
	channelbaseSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	return 1;
}

static void frpc_test_paralle(struct StackCoSche_t* sche, ChannelBase_t* channel) {
	int i;
	InnerMsg_t msg;
	char test_data[] = "test paralle ^.^";
	long long tm_msec = gmtimeMillisecond();
	StackCoBlockGroup_t group = { 0 };

	for (i = 0; i < 2; ++i) {
		StackCoBlock_t* block;
		block = StackCoSche_block_point_util(sche, tm_msec + 1000, &group);
		if (!block) {
			continue;
		}
		makeInnerMsgRpcReq(&msg, block->id, CMD_REQ_ParallelTest1, test_data, sizeof(test_data));
		channelbaseSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);

		block = StackCoSche_block_point_util(sche, tm_msec + 1000, &group);
		if (!block) {
			continue;
		}
		makeInnerMsgRpcReq(&msg, block->id, CMD_REQ_ParallelTest2, test_data, sizeof(test_data));
		channelbaseSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	}
	while (1) {
		DispatchNetMsg_t* ret_msg;
		StackCoBlock_t* block;

		block = StackCoSche_yield_group(sche, &group);
		if (!block) {
			break;
		}
		if (block->status != STACK_CO_STATUS_FINISH) {
			printf("rpc identity(%d) call failure timeout or cancel\n", block->id);
			continue;
		}
		ret_msg = (DispatchNetMsg_t*)block->resume_ret;
		printf("rpc identity(%d) return: %s ...\n", block->id, ret_msg->data);
	}
	StackCoSche_reuse_block_group(sche, &group);
}

static void test_timer(struct StackCoSche_t* sche, void* arg) {
	StackCoBlock_t* block;
	while (1) {
		logInfo(ptrBSG()->log, "test_timer============================================");
		StackCoSche_sleep_util(sche, gmtimeMillisecond() + 1000, NULL);
		block = StackCoSche_yield(sche);
		if (!block || block->status != STACK_CO_STATUS_FINISH) {
			break;
		}
		StackCoSche_reuse_block(sche, block);
	}
}

static int simply_dgram_on_read(ChannelBase_t* channel, unsigned char* buf, unsigned int len, long long timestamp_msec, const struct sockaddr* from_addr, socklen_t addrlen) {
	IPString_t ip;
	unsigned short port;
	if (!sockaddrDecode(from_addr, ip, &port)) {
		return len;
	}
	printf("reflect_udp_on_recv from %s:%hu, %u bytes, %s\n", ip, port, len, (char*)buf);
	return len;
}

static ChannelBaseProc_t s_simply_udp_proc = {
	NULL,
	simply_dgram_on_read,
	NULL,
	NULL,
	NULL,
	defaultChannelOnDetach,
	NULL
};

void test_simply_udp_client(unsigned short port) {
	char data[] = "udp hahahahhah.....";
	ChannelBase_t* c;
	Sockaddr_t saddr;
	socklen_t saddrlen = sockaddrEncode(&saddr.sa, AF_INET, "127.0.0.1", 45678);
	if (saddrlen <= 0) {
		return;
	}
	c = channelbaseOpen(0, &s_simply_udp_proc, saddr.sa.sa_family, SOCK_DGRAM, 0);
	if (!c) {
		return;
	}
	channelbaseReg(selectReactor(), c);
	channelbaseSend(c, data, sizeof(data), 0, &saddr.sa, saddrlen);
}

static void filter_dispatch(TaskThread_t* thrd, DispatchBaseMsg_t* req_ctrl) {
	if (req_ctrl->dispatch_net_msg_type) {
		DispatchNetMsg_t* net_msg = pod_container_of(req_ctrl, DispatchNetMsg_t, base);
		net_msg->callback(thrd, net_msg);
	}
}

void run(struct StackCoSche_t* sche, void* arg) {
	int i;
	StackCoBlock_t* block;
	TaskThread_t* thrd = currentTaskThread();

	// add timer
	//StackCoSche_timeout_util(sche, gmtimeMillisecond() / 1000 * 1000 + 1000, test_timer, NULL, NULL);

	ChannelBase_t* def_c = NULL;
	for (i = 0; i < ptrBSG()->conf->connect_options_cnt; ++i) {
		const ConfigConnectOption_t* option = ptrBSG()->conf->connect_options + i;
		ChannelBase_t* c;

		if (strcmp(option->protocol, "default")) {
			continue;
		}
		c = openChannelInnerClient(option->socktype, option->ip, option->port, sche);
		if (!c) {
			return;
		}
		c->on_syn_ack = defaultRpcOnSynAck;
		c->connect_timeout_sec = 5;

		logInfo(ptrBSG()->log, "channel(%p) connecting......", c);

		block = StackCoSche_block_point_util(sche, gmtimeMillisecond() + 5000, NULL);
		if (!block) {
			channelbaseCloseRef(c);
			return;
		}
		channelUserData(c)->rpc_id_syn_ack = block->id;
		channelbaseReg(selectReactor(), c);
		block = StackCoSche_yield(sche);
		if (!block || block->status != STACK_CO_STATUS_FINISH) {
			logErr(ptrBSG()->log, "channel(%p) connect %s:%u failure", c, option->ip, option->port);
			channelbaseCloseRef(c);
			return;
		}

		logInfo(ptrBSG()->log, "channel(%p) connect success......", c);
		//def_c = c;
		frpc_test_paralle(sche, c);
		if (!start_req_login_test(c)) {
			channelbaseCloseRef(c);
			return;
		}
		channelbaseCloseRef(c);
	}
	if (def_c) {
		puts("start req echo, but not display");
		frpc_req_echo(thrd, def_c, 32 << 10);
	}
	// send normal udp
	test_simply_udp_client(45678);
}

int init(BootServerGlobal_t* g) {
	// register dispatch
	regNumberDispatch(g->dispatch, CMD_NOTIFY_TEST, notifyTest);
	regNumberDispatch(g->dispatch, CMD_RET_TEST, retTest);
	regNumberDispatch(g->dispatch, CMD_RET_LOGIN_TEST, retLoginTest);

	g->default_task_thread->filter_dispatch = filter_dispatch;
	StackCoSche_function(g->default_task_thread->sche, run, NULL, NULL);
	return 0;
}
