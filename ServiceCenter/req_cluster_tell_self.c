#include "../BootServer/global.h"
#include "service_center_handler.h"
#include <stdio.h>

void reqClusterTellSelf(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		fputs("cJSON_Parse", stderr);
		return;
	}
}