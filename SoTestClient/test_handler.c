#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"

static void frpc_callback(struct StackCoSche_t* sche, void* arg) {
	printf("rpc callback ... %s\n", (const char*)arg);
}

void frpc_test_code(TaskThread_t* thrd, ChannelBase_t* channel) {
	int i;
	char test_data[] = "this text is from client ^.^";
	InnerMsg_t msg;
	long long tm_msec = gmtimeMillisecond();
	StackCoBlock_t* sub_block_arr[2];
	//
	sub_block_arr[0] = StackCoSche_block_point_util(thrd->sche, tm_msec + 1000);
	if (!sub_block_arr[0]) {
		return;
	}
	makeInnerMsgRpcReq(&msg, sub_block_arr[0]->id, CMD_REQ_TEST_CALLBACK, test_data, sizeof(test_data));
	channelbaseSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	//
	sub_block_arr[1] = StackCoSche_block_point_util(thrd->sche, tm_msec + 1000);
	if (!sub_block_arr[1]) {
		return;
	}
	makeInnerMsgRpcReq(&msg, sub_block_arr[1]->id, CMD_REQ_TEST, test_data, sizeof(test_data));
	channelbaseSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
	//
	for (i = 0; i < sizeof(sub_block_arr) / sizeof(sub_block_arr[0]); ++i) {
		UserMsg_t* ret_msg;
		StackCoBlock_t* ret_block = StackCoSche_yield(thrd->sche);
		if (!ret_block) {
			return;
		}
		if (ret_block->status != STACK_CO_STATUS_FINISH) {
			printf("rpc(%d) call failure timeout or cancel\n", ret_block->id);
			return;
		}
		ret_msg = (UserMsg_t*)ret_block->resume_ret;
		if (ret_block->id == sub_block_arr[0]->id) {
			StackCoSche_function(thrd->sche, frpc_callback, (void*)"abcdefg", NULL);
		}
		else if (ret_block->id == sub_block_arr[1]->id) {
			long long cost_msec = gmtimeMillisecond() - tm_msec;
			printf("rpc(%d) send msec=%lld time cost(%lld msec)\n", ret_block->id, tm_msec, cost_msec);
			printf("rpc(%d) say hello world ... %s\n", ret_block->id, ret_msg->data);
		}
		else {
			puts("Exception ret_co");
			return;
		}
	}
}

void notifyTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	printf("recv server test notify, recv msec = %lld\n", gmtimeMillisecond());
	// test code
	frpc_test_code(thrd, ctrl->channel);
}

void retLoginTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	StackCoBlock_t* block;

	logInfo(ptrBSG()->log, "recv: %s", (char*)ctrl->data);

	// test code
	StackCoSche_sleep_util(thrd->sche, gmtimeMillisecond() + 5000);
	block = StackCoSche_yield(thrd->sche);
	if (!block) {
		return;
	}
	if (block->status != STACK_CO_STATUS_FINISH) {
		return;
	}
	frpc_test_code(thrd, ctrl->channel);
}

void retTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	printf("say hello world ... %s, recv msec = %lld\n", ctrl->data, gmtimeMillisecond());
}
