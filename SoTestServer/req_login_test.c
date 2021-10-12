#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

void reqLoginTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	InnerMsg_t ret_msg;

	logInfo(ptrBSG()->log, "%s recv: %s", __FUNCTION__, (char*)ctrl->data);

	makeInnerMsg(&ret_msg, CMD_RET_LOGIN_TEST, NULL, 0);
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
}
