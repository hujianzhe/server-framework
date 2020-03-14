#include "global.h"
#include <stdio.h>

int reqReconnectCluster(MQRecvMsg_t* ctrl) {
	cJSON* cjson_req_root;
	MQSendMsg_t ret_msg;
	int ok;

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		fputs("cJSON_Parse", stderr);
		return 0;
	}
	printf("req: %s\n", (char*)(ctrl->data));

	ok = 0;
	do {
		cJSON* cjson_name, *cjson_ip, *cjson_port, *cjson_session_id;
		Session_t* session;
		MQCluster_t* cluster;

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

		session = getSession(cjson_session_id->valueint);
		if (!session) {
			break;
		}
		cluster = session->cluster;
		if (!cluster) {
			break;
		}
		else if (strcmp(cluster->name, cjson_name->valuestring)) {
			break;
		}
		else if (strcmp(cluster->ip, cjson_ip->valuestring)) {
			break;
		}
		else if (cluster->port != cjson_port->valueint) {
			break;
		}

		if (session->channel != ctrl->channel) {
			Channel_t* channel = sessionUnbindChannel(session);
			if (channel) {
				channelSendv(channel, NULL, 0, NETPACKET_FIN);
			}
			sessionBindChannel(session, ctrl->channel);
		}
		else {
			ReactorCmd_t* cmd = reactorNewReuseCmd(&session->channel->_, &ctrl->peer_addr);
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

	makeMQSendMsg(&ret_msg, CMD_RET_RECONNECT, NULL, 0);
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_SYN_ACK);
	puts("reconnect start");
	return 0;
}

int retReconnect(MQRecvMsg_t* ctrl) {
	ReactorCmd_t* cmd;
	ReactorPacket_t* pkg = NULL;
	Channel_t* channel = ctrl->channel;
	if (channel->_.flag & CHANNEL_FLAG_CLIENT) {
		unsigned int hdrlen, bodylen;
		bodylen = field_sizeof(MQSendMsg_t, htonl_cmd);
		hdrlen = channel->on_hdrsize(channel, bodylen);
		pkg = reactorpacketMake(NETPACKET_FRAGMENT, hdrlen, bodylen);
		if (!pkg) {
			return 1;
		}
		*(unsigned int*)(pkg->_.buf + hdrlen) = htonl(CMD_RET_RECONNECT);
	}
	cmd = reactorNewReuseFinishCmd(&channel->_, pkg);
	reactorCommitCmd(NULL, cmd);
	puts("reconnect finish");
	return 0;
}

int reqUploadCluster(MQRecvMsg_t* ctrl) {
	cJSON* cjson_req_root;
	cJSON *cjson_ret_root, *cjson_ret_array_cluster;
	MQSendMsg_t ret_msg;
	char* ret_data;
	ListNode_t* lnode;
	Session_t* session;
	MQCluster_t* cluster;
	int ok;

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		fputs("cJSON_Parse", stderr);
		return 0;
	}
	printf("req: %s\n", (char*)(ctrl->data));

	ok = 0;
	do {
		int session_id;
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

		session = (Session_t*)malloc(sizeof(Session_t));
		if (!session) {
			fputs("malloc", stderr);
			break;
		}
		session_id = allocSessionId();
		do {
			Session_t* exist_session = getSession(session_id);
			if (exist_session) {
				Channel_t* channel = sessionUnbindChannel(exist_session);
				if (channel) {
					channelSendv(channel, NULL, 0, NETPACKET_FIN);
				}
				clusterUnbindSession(exist_session->cluster);
				unregSession(exist_session);
				free(exist_session);
			}
		} while (0);

		cluster = getCluster(cjson_name->valuestring, cjson_ip->valuestring, cjson_port->valueint);
		if (cluster) {
			Session_t* cluster_session = clusterUnbindSession(cluster);
			if (cluster_session) {
				Channel_t* channel = sessionUnbindChannel(cluster_session);
				if (channel) {
					channelSendv(channel, NULL, 0, NETPACKET_FIN);
				}
				unregSession(cluster_session);
				free(cluster_session);
			}
		}
		else {
			cluster = (MQCluster_t*)malloc(sizeof(MQCluster_t));
			if (!cluster) {
				free(session);
				fputs("malloc", stderr);
				break;
			}
			strcpy(cluster->ip, cjson_ip->valuestring);
			cluster->port = cjson_port->valueint;
			if (!regCluster(cjson_name->valuestring, cluster)) {
				free(session);
				free(cluster);
				fputs("regCluster", stderr);
				break;
			}
		}
		regSession(session_id, session);
		clusterBindSession(cluster, session);
		sessionBindChannel(session, ctrl->channel);
		ok = 1;
	} while (0);
	cJSON_Delete(cjson_req_root);
	if (!ok) {
		return 0;
	}

	cjson_ret_root = cJSON_NewObject(NULL);
	cJSON_AddNewNumber(cjson_ret_root, "session_id", session->id);
	cjson_ret_array_cluster = cJSON_AddNewArray(cjson_ret_root, "cluster");
	for (lnode = g_ClusterList.head; lnode; lnode = lnode->next) {
		MQCluster_t* exist_cluster = pod_container_of(lnode, MQCluster_t, m_reg_listnode);
		if (exist_cluster != cluster) {
			cJSON* cjson_ret_object_cluster = cJSON_AddNewObject(cjson_ret_array_cluster, NULL);
			if (cjson_ret_object_cluster) {
				cJSON_AddNewString(cjson_ret_object_cluster, "name", exist_cluster->name);
				cJSON_AddNewString(cjson_ret_object_cluster, "ip", exist_cluster->ip);
				cJSON_AddNewNumber(cjson_ret_object_cluster, "port", exist_cluster->port);
			}
			if (exist_cluster->session && exist_cluster->session->channel) {
				MQSendMsg_t notify_msg;
				makeMQSendMsg(&notify_msg, CMD_NOTIFY_NEW_CLUSTER, ctrl->data, ctrl->datalen);
				channelSendv(exist_cluster->session->channel, notify_msg.iov, sizeof(notify_msg.iov) / sizeof(notify_msg.iov[0]), NETPACKET_FRAGMENT);
			}
		}
	}
	ret_data = cJSON_Print(cjson_ret_root);
	cJSON_Delete(cjson_ret_root);

	makeMQSendMsg(&ret_msg, CMD_RET_UPLOAD_CLUSTER, ret_data, strlen(ret_data));
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
	free(ret_data);
	return 0;
}

int retUploadCluster(MQRecvMsg_t* ctrl) {
	cJSON* cjson_ret_root;

	cjson_ret_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_ret_root) {
		fputs("cJSON_Parse", stderr);
		return 0;
	}

	do {
		cJSON* cjson_ret_object_cluster, *cjson_ret_array_cluster;

		cjson_ret_array_cluster = cJSON_Field(cjson_ret_root, "cluster");
		if (!cjson_ret_array_cluster) {
			break;
		}
		for (cjson_ret_object_cluster = cjson_ret_array_cluster->child; cjson_ret_object_cluster; cjson_ret_object_cluster = cjson_ret_object_cluster->next) {
			cJSON* cjson_ip, *cjson_port, *cjson_name;
			MQCluster_t* cluster;

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
			cluster = (MQCluster_t*)malloc(sizeof(MQCluster_t));
			if (!cluster) {
				break;
			}
			strcpy(cluster->ip, cjson_ip->valuestring);
			cluster->port = cjson_port->valueint;
			if (!regCluster(cjson_name->valuestring, cluster)) {
				fputs("regCluster", stderr);
				break;
			}
		}
	} while (0);
	cJSON_Delete(cjson_ret_root);

	printf("ret: %s\n", (char*)ctrl->data);
	return 0;
}

int notifyNewCluster(MQRecvMsg_t* ctrl) {
	cJSON* cjson_ntf_root;
	int ok;

	cjson_ntf_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_ntf_root) {
		fputs("cJSON_Parse", stderr);
		return 0;
	}

	ok = 0;
	do {
		MQCluster_t* cluster;
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
			cluster = (MQCluster_t*)malloc(sizeof(MQCluster_t));
			if (!cluster) {
				fputs("malloc", stderr);
				break;
			}
			strcpy(cluster->ip, cjson_ip->valuestring);
			cluster->port = cjson_port->valueint;
			if (!regCluster(cjson_name->valuestring, cluster)) {
				free(cluster);
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

int reqRemoveCluster(MQRecvMsg_t* ctrl) {
	cJSON* cjson_req_root;
	MQSendMsg_t ret_msg;

	cjson_req_root = cJSON_Parse(NULL, (char*)ctrl->data);
	if (!cjson_req_root) {
		fputs("cJSON_Parse", stderr);
		return 0;
	}

	do {
		MQCluster_t* cluster;
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
			Session_t* session = clusterUnbindSession(cluster);
			if (session) {
				Channel_t* channel = sessionUnbindChannel(session);
				if (channel) {
					channelSendv(channel, NULL, 0, NETPACKET_FIN);
				}
			}
		}
	} while (0);
	cJSON_Delete(cjson_req_root);

	makeMQSendMsg(&ret_msg, CMD_RET_REMOVE_CLUSTER, NULL, 0);
	channelSendv(ctrl->channel, ret_msg.iov, sizeof(ret_msg.iov) / sizeof(ret_msg.iov[0]), NETPACKET_FRAGMENT);
	return 0;
}

int retRemoveCluster(MQRecvMsg_t* ctrl) {
	return 0;
}