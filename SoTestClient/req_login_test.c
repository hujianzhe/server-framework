#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

void retLoginTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	cJSON* cjson_ret_root;

	logInfo(ptrBSG()->log, "recv: %s", (char*)ctrl->data);

	cjson_ret_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_ret_root) {
		logErr(ptrBSG()->log, "cJSON_Parse error");
		return;
	}

	do {
		cJSON* cjson_sessoin_id;

		cjson_sessoin_id = cJSON_Field(cjson_ret_root, "session_id");
		if (!cjson_sessoin_id) {
			logErr(ptrBSG()->log, "miss session id field");
			break;
		}
		channelSessionId(ctrl->channel) = cjson_sessoin_id->valueint;
	} while (0);
	cJSON_Delete(cjson_ret_root);

	// test code
	if (thrd->f_rpc) {
		newFiberSleepMillsecond(thrd, 5000);
		frpc_test_code(thrd, ctrl->channel);
	}
	else if (thrd->a_rpc)
		arpc_test_code(thrd, ctrl->channel);
}
