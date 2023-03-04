#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

static int start_req_login_test(ChannelBase_t* channel) {
	InnerMsg_t msg;
	makeInnerMsg(&msg, CMD_REQ_LOGIN_TEST, NULL, 0);
	channelbaseSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
	return 1;
}

static void frpc_test_paralle(struct StackCoSche_t* sche, ChannelBase_t* channel) {
	InnerMsg_t msg;
	char test_data[] = "test paralle ^.^";
	int i, cnt_sub_block = 0;
	long long tm_msec = gmtimeMillisecond();
	StackCoBlock_t* sub_block_arr[4];
	for (i = 0; i < 2; ++i) {
		StackCoBlock_t* block;
		block = StackCoSche_block_point_util(sche, tm_msec + 1000);
		if (!block) {
			continue;
		}
		makeInnerMsgRpcReq(&msg, block->id, CMD_REQ_ParallelTest1, test_data, sizeof(test_data));
		channelbaseSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		sub_block_arr[cnt_sub_block++] = block;

		block = StackCoSche_block_point_util(sche, tm_msec + 1000);
		if (!block) {
			continue;
		}
		makeInnerMsgRpcReq(&msg, block->id, CMD_REQ_ParallelTest2, test_data, sizeof(test_data));
		channelbaseSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		sub_block_arr[cnt_sub_block++] = block;
	}
	for (i = 0; i < cnt_sub_block; ++i) {
		UserMsg_t* ret_msg;
		StackCoBlock_t* block = StackCoSche_yield(sche);
		if (!block) {
			return;
		}
		if (block->status != STACK_CO_STATUS_FINISH) {
			printf("rpc identity(%d) call failure timeout or cancel\n", block->id);
			continue;
		}
		ret_msg = (UserMsg_t*)block->resume_ret;
		printf("rpc identity(%d) return: %s ...\n", block->id, ret_msg->data);
	}
}

static void test_timer(struct StackCoSche_t* sche, void* arg) {
	StackCoBlock_t* block;
	while (1) {
		logInfo(ptrBSG()->log, "test_timer============================================");
		StackCoSche_sleep_util(sche, gmtimeMillisecond() + 1000);
		block = StackCoSche_yield(sche);
		if (!block || block->status != STACK_CO_STATUS_FINISH) {
			break;
		}
		StackCoSche_reuse_block(sche, block);
	}
}

int init(int argc, char** argv) {
	return 0;
}

void run(struct StackCoSche_t* sche, void* arg) {
	int i;
	StackCoBlock_t* block;
	TaskThread_t* thrd = currentTaskThread();

	regNumberDispatch(thrd->dispatch, CMD_NOTIFY_TEST, notifyTest);
	regNumberDispatch(thrd->dispatch, CMD_RET_TEST, retTest);
	regNumberDispatch(thrd->dispatch, CMD_RET_LOGIN_TEST, retLoginTest);

	// add timer
	StackCoSche_timeout_util(sche, gmtimeMillisecond() / 1000 * 1000 + 1000, test_timer, NULL, NULL);

	for (i = 0; i < ptrBSG()->conf->connect_options_cnt; ++i) {
		const ConfigConnectOption_t* option = ptrBSG()->conf->connect_options + i;
		Sockaddr_t connect_addr;
		ChannelBase_t* c;
		int domain;

		if (strcmp(option->protocol, "default")) {
			continue;
		}
		domain = ipstrFamily(option->ip);
		if (!sockaddrEncode(&connect_addr.sa, domain, option->ip, option->port)) {
			return;
		}
		c = openChannelInner(CHANNEL_FLAG_CLIENT, INVALID_FD_HANDLE, option->socktype, &connect_addr.sa, sche);
		if (!c) {
			channelbaseClose(c);
			return;
		}
		c->on_syn_ack = defaultRpcOnSynAck;
		c->o->stream.max_connect_timeout_sec = 5;

		logInfo(ptrBSG()->log, "channel(%p) connecting......", c);

		block = StackCoSche_block_point_util(sche, gmtimeMillisecond() + 5000);
		if (!block) {
			channelbaseClose(c);
			return;
		}
		channelUserData(c)->rpc_id_syn_ack = block->id;
		channelbaseReg(selectReactor(), c);
		block = StackCoSche_yield(sche);
		if (!block || block->status != STACK_CO_STATUS_FINISH) {
			logErr(ptrBSG()->log, "channel(%p) connect %s:%u failure", c, option->ip, option->port);
			return;
		}

		logInfo(ptrBSG()->log, "channel(%p) connect success......", c);
		frpc_test_paralle(sche, c);
		if (!start_req_login_test(c)) {
			return;
		}
	}
}
