#include "util/inc/sysapi/socket.h"
#include "util/inc/component/cJSON.h"
#include "util/inc/datastruct/memfunc.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>

MQConfig_t g_Config;

#ifdef __cplusplus
extern "C" {
#endif

int initConfig(const char* path) {
	int res = 0;
	cJSON* root, *cjson;
	do {
		root = cJSON_ParseFromFile(NULL, path);
		if (!root) {
			break;
		}

		cjson = cJSON_Field(root, "cluster_name");
		if (cjson && cjson->valuestring && cjson->valuestring[0]) {
			g_Config.cluster_name = strdup(cjson->valuestring);
			if (!g_Config.cluster_name) {
				break;
			}
		}
		else {
			break;
		}

		cjson = cJSON_Field(root, "cluster_id");
		if (cjson) {
			g_Config.cluster_id = cjson->valueint;
		}
		else {
			break;
		}

		cjson = cJSON_Field(root, "port");
		if (cjson) {
			int i;
			cJSON* node;
			g_Config.portcnt = cJSON_Size(cjson);
			g_Config.port = (unsigned short*)malloc(sizeof(unsigned short) * g_Config.portcnt);
			if (!g_Config.port) {
				break;
			}
			i = 0;
			for (node = cjson->child; node; node = node->next) {
				g_Config.port[i++] = node->valueint;
			}
		}

		cjson = cJSON_Field(root, "ipv6_enable");
		if (cjson) {
			g_Config.domain = cjson->valueint ? AF_INET6 : AF_INET;
		}
		else {
			g_Config.domain = AF_INET;
		}

		cjson = cJSON_Field(root, "socktype");
		if (cjson) {
			if (!strcmp(cjson->valuestring, "SOCK_STREAM"))
				g_Config.socktype = SOCK_STREAM;
			else if (!strcmp(cjson->valuestring, "SOCK_DGRAM"))
				g_Config.socktype = SOCK_DGRAM;
			else
				break;
		}
		else {
			g_Config.socktype = SOCK_STREAM;
		}

		cjson = cJSON_Field(root, "local_ip");
		if (cjson) {
			strcpy(g_Config.local_ip, cjson->valuestring);
		}

		cjson = cJSON_Field(root, "listen_ip");
		if (cjson) {
			strcpy(g_Config.listen_ip, cjson->valuestring);
		}

		cjson = cJSON_Field(root, "outer_ip");
		if (cjson) {
			strcpy(g_Config.outer_ip, cjson->valuestring);
		}

		cjson = cJSON_Field(root, "timer_interval_msec");
		if (cjson) {
			g_Config.timer_interval_msec = cjson->valueint;
		}
		else {
			g_Config.timer_interval_msec = 16;
		}

		cjson = cJSON_Field(root, "center_addr");
		if (cjson) {
			cJSON* sub_cjson = cJSON_Field(cjson, "ip");
			if (sub_cjson)
				strcpy(g_Config.center_attr.ip, sub_cjson->valuestring);
			sub_cjson = cJSON_Field(cjson, "port");
			if (sub_cjson)
				g_Config.center_attr.port = sub_cjson->valueint;
			sub_cjson = cJSON_Field(cjson, "socktype");
			if (sub_cjson) {
				if (!strcmp(sub_cjson->valuestring, "SOCK_STREAM"))
					g_Config.center_attr.socktype = SOCK_STREAM;
				else if (!strcmp(sub_cjson->valuestring, "SOCK_DGRAM"))
					g_Config.center_attr.socktype = SOCK_DGRAM;
				else
					break;
			}
			else {
				g_Config.center_attr.socktype = SOCK_STREAM;
			}
		}

		res = 1;
	} while (0);
	cJSON_Delete(root);
	return res;
}

void freeConfig(void) {
	free(g_Config.port);
	free((char*)(g_Config.cluster_name));
	g_Config.port = NULL;
	g_Config.portcnt = 0;
	g_Config.cluster_name = NULL;
	g_Config.outer_ip[0] = 0;
	g_Config.center_attr.ip[0] = 0;
}

#ifdef __cplusplus
}
#endif
