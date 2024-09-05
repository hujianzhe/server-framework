#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

void reqLoginTest(TaskThread_t* thrd, DispatchNetMsg_t* ctrl) {
	InnerMsgPayload_t ret_msg;

	makeInnerMsg(&ret_msg, CMD_RET_LOGIN_TEST, NULL, 0);
	NetChannel_sendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
}
