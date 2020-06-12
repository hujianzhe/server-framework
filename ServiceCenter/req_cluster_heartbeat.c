#include "../BootServer/global.h"
#include "../ServiceCommCode/service_comm_cmd.h"
#include "service_center_handler.h"

void reqClusterHeartbeat(TaskThread_t* thrd, UserMsg_t* ctrl) {
	cJSON* cjson_root;
	cJSON *cjson_ip, *cjson_port, *cjson_weight_num, *cjson_connection_num;

	logInfo(ptr_g_Log(), "%s req: %s", __FUNCTION__, (char*)(ctrl->data));

	cjson_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_root) {
		logErr(ptr_g_Log(), "%s cJSON_Parse err", __FUNCTION__);
		return;
	}

	cjson_ip = cJSON_Field(cjson_root, "ip");
	if (!cjson_ip) {
		return;
	}
	cjson_port = cJSON_Field(cjson_root, "port");
	if (!cjson_port) {
		return;
	}
	cjson_weight_num = cJSON_Field(cjson_root, "weight_num");
	if (!cjson_weight_num) {
		return;
	}
	cjson_connection_num = cJSON_Field(cjson_root, "connection_num");
	if (!cjson_connection_num) {
		return;
	}
	// TODO flush cluster heartbeat
}