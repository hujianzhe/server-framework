#include "../BootServer/global.h"
#include "mq_cmd.h"
#include "mq_handler.h"
#include <stdio.h>

int reqReconnectCluster(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;
	SendMsg_t ret_msg;
	int ok;

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		fputs("cJSON_Parse", stderr);
		return 0;
	}
	printf("req: %s\n", (char*)(ctrl->data));

	ok = 0;
	do {
		Cluster_t* cluster;
		cJSON* cjson_name, *cjson_ip, *cjson_port, *cjson_session_id;

		cjson_name = cJSON_Field(cjson_req_root, "name");
		if (!cjson_name) {
			break;
		}
		cjson_ip = cJSON_Field(cjson_req_root, "ip");
		if (!cjson_ip) {
			break;
		}
		cjson_port = cJSON_Field(cjson_req_root, "port");
		if (!cjson_port) {
			break;
		}
		cjson_session_id = cJSON_Field(cjson_req_root, "session_id");
		if (!cjson_session_id) {
			break;
		}

		cluster = getCluster(cjson_name->valuestring, cjson_ip->valuestring, cjson_port->valueint);
		if (!cluster) {
			break;
		}
		if (cluster->session.id != cjson_session_id->valueint) {
			break;
		}

		if (cluster->session.channel != ctrl->channel) {
			Channel_t* channel = sessionUnbindChannel(&cluster->session);
			if (channel) {
				channelSendv(channel, NULL, 0, NETPACKET_FIN);
			}
			sessionBindChannel(&cluster->session, ctrl->channel);
		}
		else {
			ReactorCmd_t* cmd;
			IPString_t ip = { 0 };
			unsigned short port = 0;
			if (!sockaddrDecode(&ctrl->peer_addr.st, ip, &port)) {
				break;
			}
			printf("recv client reconnect (%s:%hu)\n", ip, port);
			cmd = reactorNewReuseCmd(&cluster->session.channel->_, &ctrl->peer_addr);
			if (!cmd) {
				break;
			}
			reactorCommitCmd(NULL, cmd);
		}
		ok = 1;
	} while (0);
	cJSON_Delete(cjson_req_root);
	if (!ok) {
		channelSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
		puts("reconnect failure");
		return 1;
	}

	makeSendMsg(&ret_msg, CMD_RET_RECONNECT, NULL, 0);
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_SYN_ACK);
	puts("reconnect start");
	return 0;
}

int retReconnect(UserMsg_t* ctrl) {
	ReactorCmd_t* cmd;
	ReactorPacket_t* pkg = NULL;
	Channel_t* channel = ctrl->channel;
	if (channel->_.flag & CHANNEL_FLAG_CLIENT) {
		List_t pklist;
		SendMsg_t ret_msg;
		makeSendMsg(&ret_msg, CMD_RET_RECONNECT, NULL, 0);
		listInit(&pklist);
		if (!channelShard(channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT_EOF, &pklist)) {
			puts("reconnect error");
			return 0;
		}
		pkg = pod_container_of(pklist.head, ReactorPacket_t, cmd._);
	}
	cmd = reactorNewReuseFinishCmd(&channel->_, pkg);
	reactorCommitCmd(NULL, cmd);
	puts("reconnect finish");
	return 0;
}

int reqUploadCluster(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;
	cJSON *cjson_ret_root, *cjson_ret_array_cluster;
	SendMsg_t ret_msg;
	char* ret_data;
	ListNode_t* lnode;
	Cluster_t* cluster;
	int ok;

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		fputs("cJSON_Parse", stderr);
		return 0;
	}
	printf("req: %s\n", (char*)(ctrl->data));

	ok = 0;
	do {
		cJSON* cjson_name, *cjson_ip, *cjson_port;

		cjson_name = cJSON_Field(cjson_req_root, "name");
		if (!cjson_name) {
			break;
		}
		cjson_ip = cJSON_Field(cjson_req_root, "ip");
		if (!cjson_ip) {
			break;
		}
		cjson_port = cJSON_Field(cjson_req_root, "port");
		if (!cjson_port) {
			break;
		}

		cluster = getCluster(cjson_name->valuestring, cjson_ip->valuestring, cjson_port->valueint);
		if (cluster) {
			Channel_t* channel = sessionUnbindChannel(&cluster->session);
			if (channel) {
				channelSendv(channel, NULL, 0, NETPACKET_FIN);
			}
		}
		else {
			cluster = newCluster();
			if (!cluster) {
				break;
			}
			strcpy(cluster->ip, cjson_ip->valuestring);
			cluster->port = cjson_port->valueint;
			if (!regCluster(cjson_name->valuestring, cluster)) {
				freeCluster(cluster);
				channelSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
				fputs("regCluster", stderr);
				break;
			}
		}
		cluster->session.id = allocSessionId();
		sessionBindChannel(&cluster->session, ctrl->channel);
		ok = 1;
	} while (0);
	cJSON_Delete(cjson_req_root);
	if (!ok) {
		return 0;
	}

	cjson_ret_root = cJSON_NewObject(NULL);
	cJSON_AddNewNumber(cjson_ret_root, "session_id", cluster->session.id);
	cjson_ret_array_cluster = cJSON_AddNewArray(cjson_ret_root, "cluster");
	for (lnode = ptr_g_ClusterList()->head; lnode; lnode = lnode->next) {
		Cluster_t* exist_cluster = pod_container_of(lnode, Cluster_t, m_listnode);
		if (exist_cluster != cluster) {
			cJSON* cjson_ret_object_cluster = cJSON_AddNewObject(cjson_ret_array_cluster, NULL);
			if (cjson_ret_object_cluster) {
				cJSON_AddNewString(cjson_ret_object_cluster, "name", exist_cluster->name);
				cJSON_AddNewString(cjson_ret_object_cluster, "ip", exist_cluster->ip);
				cJSON_AddNewNumber(cjson_ret_object_cluster, "port", exist_cluster->port);
			}
			if (exist_cluster->session.channel) {
				SendMsg_t notify_msg;
				makeSendMsg(&notify_msg, CMD_NOTIFY_NEW_CLUSTER, ctrl->data, ctrl->datalen);
				channelSendv(exist_cluster->session.channel, notify_msg.iov, sizeof(notify_msg.iov) / sizeof(notify_msg.iov[0]), NETPACKET_FRAGMENT);
			}
		}
	}
	ret_data = cJSON_Print(cjson_ret_root);
	cJSON_Delete(cjson_ret_root);

	makeSendMsg(&ret_msg, CMD_RET_UPLOAD_CLUSTER, ret_data, strlen(ret_data));
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
	free(ret_data);
	return 0;
}

int retUploadCluster(UserMsg_t* ctrl) {
	cJSON* cjson_ret_root;

	cjson_ret_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_ret_root) {
		fputs("cJSON_Parse", stderr);
		return 0;
	}

	do {
		cJSON* cjson_ret_object_cluster, *cjson_ret_array_cluster, *cjson_sessoin_id;

		cjson_sessoin_id = cJSON_Field(cjson_ret_root, "session_id");
		if (!cjson_sessoin_id) {
			fputs("miss session id field", stderr);
			break;
		}

		cjson_ret_array_cluster = cJSON_Field(cjson_ret_root, "cluster");
		if (!cjson_ret_array_cluster) {
			break;
		}
		for (cjson_ret_object_cluster = cjson_ret_array_cluster->child; cjson_ret_object_cluster; cjson_ret_object_cluster = cjson_ret_object_cluster->next) {
			cJSON* cjson_ip, *cjson_port, *cjson_name;
			Cluster_t* cluster;

			cjson_ip = cJSON_Field(cjson_ret_object_cluster, "ip");
			if (!cjson_ip) {
				continue;
			}
			cjson_port = cJSON_Field(cjson_ret_object_cluster, "port");
			if (!cjson_port) {
				continue;
			}
			cjson_name = cJSON_Field(cjson_ret_object_cluster, "name");
			if (!cjson_name) {
				continue;
			}

			cluster = getCluster(cjson_name->valuestring, cjson_ip->valuestring, cjson_port->valueint);
			if (cluster) {
				continue;
			}
			cluster = newCluster();
			if (!cluster) {
				break;
			}
			strcpy(cluster->ip, cjson_ip->valuestring);
			cluster->port = cjson_port->valueint;
			if (!regCluster(cjson_name->valuestring, cluster)) {
				freeCluster(cluster);
				channelSendv(ctrl->channel, NULL, 0, NETPACKET_FIN);
				fputs("regCluster", stderr);
				break;
			}
		}
		channelSessionId(ctrl->channel) = cjson_sessoin_id->valueint;
	} while (0);
	cJSON_Delete(cjson_ret_root);

	printf("ret: %s\n", (char*)ctrl->data);

	// test code
	if (ptr_g_RpcFiberCore())
		frpc_test_code(ctrl->channel);
	else if (ptr_g_RpcAsyncCore())
		arpc_test_code(ctrl->channel);

	return 0;
}

int notifyNewCluster(UserMsg_t* ctrl) {
	cJSON* cjson_ntf_root;
	int ok;

	cjson_ntf_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_ntf_root) {
		fputs("cJSON_Parse", stderr);
		return 0;
	}

	ok = 0;
	do {
		Cluster_t* cluster;
		cJSON* cjson_name, *cjson_ip, *cjson_port;

		cjson_name = cJSON_Field(cjson_ntf_root, "name");
		if (!cjson_name) {
			break;
		}
		cjson_ip = cJSON_Field(cjson_ntf_root, "ip");
		if (!cjson_ip) {
			break;
		}
		cjson_port = cJSON_Field(cjson_ntf_root, "port");
		if (!cjson_port) {
			break;
		}

		cluster = getCluster(cjson_name->valuestring, cjson_ip->valuestring, cjson_port->valueint);
		if (!cluster) {
			cluster = newCluster();
			if (!cluster) {
				break;
			}
			strcpy(cluster->ip, cjson_ip->valuestring);
			cluster->port = cjson_port->valueint;
			if (!regCluster(cjson_name->valuestring, cluster)) {
				freeCluster(cluster);
				fputs("regCluster", stderr);
				break;
			}
		}
		ok = 1;
	} while (0);
	cJSON_Delete(cjson_ntf_root);
	if (!ok) {
		return 0;
	}

	printf("notify: %s\n", (char*)ctrl->data);
	return 0;
}

int reqRemoveCluster(UserMsg_t* ctrl) {
	cJSON* cjson_req_root;
	SendMsg_t ret_msg;

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		fputs("cJSON_Parse", stderr);
		return 0;
	}

	do {
		Cluster_t* cluster;
		cJSON* cjson_name, *cjson_ip, *cjson_port;

		cjson_name = cJSON_Field(cjson_req_root, "name");
		if (!cjson_name) {
			break;
		}
		cjson_ip = cJSON_Field(cjson_req_root, "ip");
		if (!cjson_ip) {
			break;
		}
		cjson_port = cJSON_Field(cjson_req_root, "port");
		if (!cjson_port) {
			break;
		}

		cluster = getCluster(cjson_name->valuestring, cjson_ip->valuestring, cjson_port->valueint);
		if (cluster) {
			Channel_t* channel = sessionUnbindChannel(&cluster->session);
			if (channel) {
				channelSendv(channel, NULL, 0, NETPACKET_FIN);
			}
			unregCluster(cluster);
		}
	} while (0);
	cJSON_Delete(cjson_req_root);

	makeSendMsg(&ret_msg, CMD_RET_REMOVE_CLUSTER, NULL, 0);
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
	return 0;
}

int retRemoveCluster(UserMsg_t* ctrl) {
	return 0;
}