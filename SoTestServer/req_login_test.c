#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

void reqLoginTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	cJSON *cjson_ret_root;
	InnerMsg_t ret_msg;

	logInfo(ptrBSG()->log, "%s recv: %s", __FUNCTION__, (char*)ctrl->data);

	channelSessionId(ctrl->channel) = allocSessionId();

	cjson_ret_root = cJSON_NewObject(NULL);
	cJSON_AddNewNumber(cjson_ret_root, "session_id", channelSessionId(ctrl->channel));
	cJSON_Print(cjson_ret_root);

	makeInnerMsg(&ret_msg, CMD_RET_LOGIN_TEST, cjson_ret_root->txt, cjson_ret_root->txtlen);
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
	cJSON_Delete(cjson_ret_root);
}
