#include "../BootServer/global.h"
#include "../ServiceCommCode/service_comm_cmd.h"
#include "service_center_handler.h"

void reqClusterHeartbeat(TaskThread_t* thrd, UserMsg_t* ctrl) {
	cJSON* cjson_root;
	cJSON *cjson_name, *cjson_ip, *cjson_port, *cjson_weight_num, *cjson_connection_num;
	Cluster_t* cluster;

	logInfo(ptr_g_Log(), "%s req: %s", __FUNCTION__, (char*)(ctrl->data));

	cjson_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_root) {
		logErr(ptr_g_Log(), "%s cJSON_Parse err", __FUNCTION__);
		return;
	}

	cjson_name = cJSON_Field(cjson_root, "name");
	if (!cjson_name) {
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

	cluster = getCluster(ptr_g_ClusterTable(), cjson_name->valuestring, cjson_ip->valuestring, cjson_port->valueint);
	if (cluster) {
		cluster->weight_num = cjson_weight_num->valueint;
		cluster->connection_num = cjson_connection_num->valueint;
		logInfo(ptr_g_Log(), "%s flush name(%s) ip(%s) port(%u) weight_num(%d) connection_num(%d)", __FUNCTION__,
			cluster->name, cluster->ip, cluster->port, cluster->weight_num, cluster->connection_num);
	}
}