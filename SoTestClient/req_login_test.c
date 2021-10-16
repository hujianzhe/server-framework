#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

void retLoginTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	logInfo(ptrBSG()->log, "recv: %s", (char*)ctrl->data);

	// test code
	if (thrd->f_rpc) {
		newFiberSleepMillsecond(5000);
		frpc_test_code(thrd, ctrl->channel);
	}
	else if (thrd->a_rpc)
		arpc_test_code(thrd, ctrl->channel);
}
