#include "../BootServer/global.h"
#include "service_center_handler.h"

void reqChangeClusterNode_http(UserMsg_t* ctrl) {
	cJSON* root;
	int retcode = 0;

	root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!root) {
		logErr(ptr_g_Log(), "Config parse extra data error");
		retcode = 1;
		goto err;
	}

	cJSON_Delete(root);
	return;
err:
	cJSON_Delete(root);
}