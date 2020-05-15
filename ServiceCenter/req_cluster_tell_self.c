#include "../BootServer/global.h"
#include "service_center_handler.h"
#include <stdio.h>

void reqClusterTellSelf(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;

	if (!cJSON_Parse(NULL, ctrl->data)) {
		fputs("cJSON_Parse", stderr);
		return;
	}

	
}