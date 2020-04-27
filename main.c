#include "global.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int THREAD_CALL reactorThreadEntry(void* arg);
unsigned int THREAD_CALL taskThreadEntry(void* arg);

static void sigintHandler(int signo) {
	int i;
	g_Valid = 0;
	dataqueueWake(&g_DataQueue);
	reactorWake(g_ReactorAccept);
	for (i = 0; i < g_ReactorCnt; ++i) {
		reactorWake(g_Reactors + i);
	}
}

static int centerChannelHeartbeat(Channel_t* c, int heartbeat_times) {
	if (heartbeat_times < c->heartbeat_maxtimes) {
		SendMsg_t msg;
		makeSendMsgEmpty(&msg);
		channelShardSendv(c, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_NO_ACK_FRAGMENT);
		printf("channel(%p) send heartbeat, times %d...\n", c, heartbeat_times);
	}
	else {
		ReactorCmd_t* cmd;
		printf("channel(%p) zombie...\n", c);
		cmd = reactorNewReuseCmd(&c->_, NULL);
		if (!cmd) {
			return 0;
		}
		reactorCommitCmd(NULL, cmd);
		printf("channel(%p) reconnect start...\n", c);
	}
	return 1;
}

static void centerChannelConnectCallback(ChannelBase_t* c, long long ts_msec) {
	Channel_t* channel = pod_container_of(c, Channel_t, _);
	char buffer[1024];
	SendMsg_t msg;
	IPString_t peer_ip = { 0 };
	unsigned short peer_port = 0;

	channelEnableHeartbeat(channel, ts_msec);

	sockaddrDecode(&c->to_addr.st, peer_ip, &peer_port);
	printf("channel(%p) connect success, ip:%s, port:%hu\n", c, peer_ip, peer_port);

	if (c->connected_times > 1) {
		unsigned short port = g_Config.listen_options ? g_Config.listen_options[0].port : 0;
		sprintf(buffer, "{\"name\":\"%s\",\"ip\":\"%s\",\"port\":%u,\"session_id\":%d}", g_Config.cluster_name, g_Config.outer_ip, port, channelSessionId(channel));
		makeSendMsg(&msg, CMD_REQ_RECONNECT, buffer, strlen(buffer));
		channelShardSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_SYN);
	}
	else {
		unsigned short port = g_Config.listen_options ? g_Config.listen_options[0].port : 0;
		sprintf(buffer, "{\"name\":\"%s\",\"ip\":\"%s\",\"port\":%u}", g_Config.cluster_name, g_Config.outer_ip, port);
		makeSendMsg(&msg, CMD_REQ_UPLOAD_CLUSTER, buffer, strlen(buffer));
		channelShardSendv(channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT);
		/*
		int i = 0;
		for (i = 0; i < sizeof(buffer); ++i) {
			buffer[i] = i % 255;
		}
		channelSend(channel, buffer, sizeof(buffer), NETPACKET_FRAGMENT);
		*/
	}
}

int main(int argc, char** argv) {
	int i;
	int dqinitok = 0, timerinitok = 0, timerrpcinitok = 0,
		taskthreadinitok = 0, socketloopinitokcnt = 0,
		acceptthreadinitok = 0, acceptloopinitok = 0,
		listensockinitokcnt = 0, connectsockinitokcnt = 0;
	//
	if (!initConfig(argc > 1 ? argv[1] : "config.txt")) {
		return 1;
	}
	printf("cluster_name:%s, pid:%zu\n", g_Config.cluster_name, processId());

	if (!initGlobalResource()) {
		return 1;
	}

	initDispatch();
	initClusterTable();
	initSessionTable();

	if (!dataqueueInit(&g_DataQueue))
		goto err;
	dqinitok = 1;

	if (!rbtimerInit(&g_Timer, TRUE))
		goto err;
	timerinitok = 1;
	if (!rbtimerInit(&g_TimerRpcTimeout, TRUE))
		goto err;
	timerrpcinitok = 1;

	if (!threadCreate(&g_TaskThread, taskThreadEntry, NULL))
		goto err;
	taskthreadinitok = 1;

	if (!reactorInit(g_ReactorAccept))
		goto err;
	acceptloopinitok = 1;

	if (!threadCreate(g_ReactorAcceptThread, reactorThreadEntry, g_ReactorAccept))
		goto err;
	acceptthreadinitok = 1;

	for (; socketloopinitokcnt < g_ReactorCnt; ++socketloopinitokcnt) {
		if (!reactorInit(g_Reactors + socketloopinitokcnt)) {
			goto err;
		}
		if (!threadCreate(g_ReactorThreads + socketloopinitokcnt, reactorThreadEntry, g_Reactors + socketloopinitokcnt)) {
			goto err;
		}
	}

	if (signalRegHandler(SIGINT, sigintHandler) == SIG_ERR)
		goto err;

	regNumberDispatch(CMD_REQ_TEST, reqTest);
	regNumberDispatch(CMD_NOTIFY_TEST, notifyTest);
	regNumberDispatch(CMD_RET_TEST, retTest);
	regNumberDispatch(CMD_REQ_RECONNECT, reqReconnectCluster);
	regNumberDispatch(CMD_RET_RECONNECT, retReconnect);
	regNumberDispatch(CMD_REQ_UPLOAD_CLUSTER, reqUploadCluster);
	regNumberDispatch(CMD_RET_UPLOAD_CLUSTER, retUploadCluster);
	regNumberDispatch(CMD_NOTIFY_NEW_CLUSTER, notifyNewCluster);
	regNumberDispatch(CMD_REQ_REMOVE_CLUSTER, reqRemoveCluster);
	regNumberDispatch(CMD_RET_REMOVE_CLUSTER, retRemoveCluster);
	regStringDispatch("/reqHttpTest", reqHttpTest);

	for (listensockinitokcnt = 0; listensockinitokcnt < g_Config.listen_options_cnt; ++listensockinitokcnt) {
		ConfigListenOption_t* option = g_Config.listen_options + listensockinitokcnt;
		if (!strcmp(option->protocol, "inner")) {
			int domain = ipstrFamily(option->ip);
			ReactorObject_t* o = openListener(domain, option->socktype, option->ip, option->port);
			if (!o)
				goto err;
			reactorCommitCmd(g_ReactorAccept, &o->regcmd);
		}
		else if (!strcmp(option->protocol, "http")) {
			int domain = ipstrFamily(option->ip);
			ReactorObject_t* o = openListenerHttp(domain, option->ip, option->port);
			if (!o)
				goto err;
			reactorCommitCmd(g_ReactorAccept, &o->regcmd);
		}
	}

	for (connectsockinitokcnt = 0; connectsockinitokcnt < g_Config.connect_options_cnt; ++connectsockinitokcnt) {
		ConfigConnectOption_t* option = g_Config.connect_options + connectsockinitokcnt;
		Sockaddr_t connect_addr;
		Channel_t* c;
		ReactorObject_t* o;
		if (strcmp(option->protocol, "inner")) {
			continue;
		}
		if (!sockaddrEncode(&connect_addr.st, ipstrFamily(option->ip), option->ip, option->port))
			goto err;
		o = reactorobjectOpen(INVALID_FD_HANDLE, connect_addr.st.ss_family, option->socktype, 0);
		if (!o)
			goto err;
		c = openChannel(o, CHANNEL_FLAG_CLIENT, &connect_addr);
		if (!c) {
			reactorCommitCmd(NULL, &o->freecmd);
			goto err;
		}
		if (!strcmp(option->protocol, "inner")) {
			c->_.on_syn_ack = centerChannelConnectCallback;
			c->on_heartbeat = centerChannelHeartbeat;
		}
		printf("channel(%p) connecting......\n", c);
		reactorCommitCmd(selectReactor((size_t)(o->fd)), &o->regcmd);
	}
	//
	threadJoin(g_TaskThread, NULL);
	threadJoin(*g_ReactorAcceptThread, NULL);
	reactorDestroy(g_ReactorAccept);
	for (i = 0; i < g_ReactorCnt; ++i) {
		threadJoin(g_ReactorThreads[i], NULL);
		reactorDestroy(g_Reactors + i);
	}
	goto end;
err:
	g_Valid = 0;
	if (acceptthreadinitok) {
		threadJoin(*g_ReactorAcceptThread, NULL);
	}
	if (acceptloopinitok) {
		reactorDestroy(g_ReactorAccept);
	}
	while (socketloopinitokcnt--) {
		threadJoin(g_ReactorThreads[socketloopinitokcnt], NULL);
		reactorDestroy(g_Reactors + socketloopinitokcnt);
	}
	if (taskthreadinitok) {
		dataqueueWake(&g_DataQueue);
		threadJoin(g_TaskThread, NULL);
	}
end:
	if (dqinitok) {
		dataqueueDestroy(&g_DataQueue);
	}
	if (timerinitok) {
		rbtimerDestroy(&g_Timer);
	}
	if (timerrpcinitok) {
		rbtimerDestroy(&g_TimerRpcTimeout);
	}
	freeConfig();
	freeDispatchCallback();
	freeSessionTable();
	freeClusterTable();
	freeGlobalResource();
	return 0;
}
