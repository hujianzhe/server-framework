#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

static int start_req_login_test(NetChannel_t* channel) {
	InnerMsgPayload_t msg;
	makeInnerMsg(&msg, CMD_REQ_LOGIN_TEST, NULL, 0);
	NetChannel_sendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	return 1;
}

static void frpc_test_paralle(struct StackCoSche_t* sche, NetChannel_t* channel) {
	int i;
	InnerMsgPayload_t msg;
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
		NetChannel_sendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);

		block = StackCoSche_block_point_util(sche, tm_msec + 1000, &group);
		if (!block) {
			continue;
		}
		makeInnerMsgRpcReq(&msg, block->id, CMD_REQ_ParallelTest2, test_data, sizeof(test_data));
		NetChannel_sendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	}
	while (!StackCoSche_group_is_empty(&group)) {
		DispatchNetMsg_t* ret_msg;
		StackCoBlock_t* block;

		block = StackCoSche_yield_group(sche, &group);
		if (StackCoSche_has_exit(sche)) {
			puts("thread coroutine sche has exit");
			break;
		}
		if (block->status != STACK_CO_STATUS_FINISH) {
			printf("rpc identity(%d) call failure timeout or cancel\n", block->id);
			continue;
		}
		ret_msg = (DispatchNetMsg_t*)block->resume_param.value;
		printf("rpc identity(%d) return: %s ...\n", block->id, ret_msg->data);
	}
	StackCoSche_reuse_block_group(sche, &group);
}

static void test_timer(struct StackCoSche_t* sche, StackCoAsyncParam_t* param) {
	StackCoBlock_t* block;
	while (1) {
		logInfo(ptrBSG()->log, "test_timer============================================");
		StackCoSche_sleep_util(sche, gmtimeMillisecond() + 1000, NULL);
		block = StackCoSche_yield(sche);
		if (StackCoSche_has_exit(sche)) {
			puts("thread coroutine sche has exit");
			break;
		}
		if (block->status != STACK_CO_STATUS_FINISH) {
			break;
		}
		StackCoSche_reuse_block(sche, block);
	}
}

static int simply_dgram_on_read(NetChannel_t* channel, unsigned char* buf, unsigned int len, long long timestamp_msec, const struct sockaddr* from_addr, socklen_t addrlen) {
	IPString_t ip;
	unsigned short port;
	if (!sockaddrDecode(from_addr, ip, &port)) {
		return len;
	}
	printf("reflect_udp_on_recv from %s:%hu, %u bytes, %s\n", ip, port, len, (char*)buf);
	return len;
}

static NetChannelProc_t s_simply_udp_proc = {
	NULL,
	simply_dgram_on_read,
	NULL,
	NULL,
	NULL,
	defaultNetChannelOnDetach,
	NULL
};

void test_simply_udp_client(unsigned short port) {
	char data[] = "udp hahahahhah.....";
	NetChannel_t* c;
	Sockaddr_t saddr;
	socklen_t saddrlen = sockaddrEncode(&saddr.sa, AF_INET, "127.0.0.1", 45678);
	if (saddrlen <= 0) {
		return;
	}
	c = NetChannel_open(0, &s_simply_udp_proc, saddr.sa.sa_family, SOCK_DGRAM, 0);
	if (!c) {
		return;
	}
	NetChannel_reg(selectNetReactor(), c);
	NetChannel_send(c, data, sizeof(data), 0, &saddr.sa, saddrlen);
}

void run(struct StackCoSche_t* sche, StackCoAsyncParam_t* param) {
	int i;
	StackCoBlock_t* block;
	TaskThread_t* thrd = currentTaskThread();

	// add timer
	// StackCoSche_timeout_util(sche, gmtimeMillisecond() / 1000 * 1000 + 1000, test_timer, NULL);

	NetChannel_t* def_c = NULL;
	for (i = 0; i < ptrBSG()->conf->connect_options_cnt; ++i) {
		const ConfigConnectOption_t* option = ptrBSG()->conf->connect_options + i;
		NetChannel_t* c;

		if (strcmp(option->protocol, "default")) {
			continue;
		}
		c = openNetChannelInnerClient(option->socktype, option->ip, option->port, sche);
		if (!c) {
			return;
		}
		c->on_syn_ack = defaultNetChannelOnSynAck;
		c->connect_timeout_sec = 5;

		logInfo(ptrBSG()->log, "channel(%p) connecting......", c);

		block = StackCoSche_block_point_util(sche, gmtimeMillisecond() + 5000, NULL);
		if (!block) {
			NetChannel_close_ref(c);
			return;
		}
		NetChannel_get_userdata(c)->rpc_id_syn_ack = block->id;
		NetChannel_reg(selectNetReactor(), c);
		block = StackCoSche_yield(sche);
		if (StackCoSche_has_exit(sche)) {
			logErr(ptrBSG()->log, "task coroutine sche has exit...");
			NetChannel_close_ref(c);
			return;
		}
		if (block->status != STACK_CO_STATUS_FINISH) {
			logErr(ptrBSG()->log, "channel(%p) connect %s:%u failure", c, option->ip, option->port);
			NetChannel_close_ref(c);
			return;
		}

		logInfo(ptrBSG()->log, "channel(%p) connect success......", c);
		//def_c = c;
		frpc_test_paralle(sche, c);
		if (!start_req_login_test(c)) {
			NetChannel_close_ref(c);
			return;
		}
		NetChannel_close_ref(c);
	}
	if (def_c) {
		puts("start req echo, but not display");
		frpc_req_echo(thrd, def_c, 32 << 10);
	}
	// send normal udp
	test_simply_udp_client(45678);
}

int init(void) {
	// register dispatch
	regNumberDispatch(ptrBSG()->dispatch, CMD_NOTIFY_TEST, notifyTest);
	regNumberDispatch(ptrBSG()->dispatch, CMD_RET_TEST, retTest);
	regNumberDispatch(ptrBSG()->dispatch, CMD_RET_LOGIN_TEST, retLoginTest);

	StackCoSche_function(ptrBSG()->default_task_thread->sche_stack_co, run, NULL);
	return 0;
}
