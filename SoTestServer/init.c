#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

static void reflect_websocket_on_recv(ChannelBase_t* channel, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* saddr, socklen_t addrlen) {
	printf("reflect_websocket_on_recv: %zu bytes\n", bodylen);
	channelbaseSend(channel, bodyptr, bodylen, NETPACKET_FRAGMENT, saddr, addrlen);
}

static int simply_dgram_on_read(ChannelBase_t* channel, unsigned char* buf, unsigned int len, long long timestamp_msec, const struct sockaddr* from_addr, socklen_t addrlen) {
	IPString_t ip;
	unsigned short port;
	if (!sockaddrDecode(from_addr, ip, &port)) {
		return len;
	}
	printf("reflect_udp_on_recv from %s:%hu, %u bytes, %s\n", ip, port, len, (char*)buf);
	channelbaseSend(channel, buf, len, 0, from_addr, addrlen);
	return len;
}

static ChannelBaseProc_t s_simply_udp_proc = {
	NULL,
	NULL,
	simply_dgram_on_read,
	NULL,
	NULL,
	NULL,
	defaultChannelOnDetach,
	NULL
};

void test_simply_udp_server(unsigned short port) {
	FD_t fd;
	ChannelBase_t* c;
	Sockaddr_t saddr;

	if (!sockaddrEncode(&saddr.sa, AF_INET, NULL, 45678)) {
		return;
	}
	fd = socket(saddr.sa.sa_family, SOCK_DGRAM, 0);
	if (INVALID_FD_HANDLE == fd) {
		return;
	}
	if (!socketBindAndReuse(fd, &saddr.sa, sockaddrLength(&saddr.sa))) {
		socketClose(fd);
		return;
	}
	c = channelbaseOpen(0, &s_simply_udp_proc, fd, AF_INET, SOCK_DGRAM, NULL);
	if (!c) {
		socketClose(fd);
		return;
	}
	channelbaseReg(selectReactor(), c);
}

void run(struct StackCoSche_t* sche, void* arg) {
	TaskThread_t* thrd = currentTaskThread();
	int i;
	// listen extra port
	for (i = 0; i < ptrBSG()->conf->listen_options_cnt; ++i) {
		const ConfigListenOption_t* option = ptrBSG()->conf->listen_options + i;
		ChannelBase_t* c;
		if (!strcmp(option->protocol, "http")) {
			c = openListenerHttp(option->ip, option->port, sche);
		}
		else if (!strcmp(option->protocol, "websocket")) {
			c = openListenerWebsocket(option->ip, option->port, reflect_websocket_on_recv, sche);
		}
		else {
			continue;
		}
		if (!c) {
			logErr(ptrBSG()->log, "listen failure, ip:%s, port:%u ......", option->ip, option->port);
			return;
		}
		channelbaseReg(acceptReactor(), c);
	}
	// listen normal udp
	test_simply_udp_server(45678);
}

int init(BootServerGlobal_t* g) {
	// register dispatch
	regNumberDispatch(g->dispatch, CMD_REQ_TEST, reqTest);
	regNumberDispatch(g->dispatch, CMD_REQ_TEST_CALLBACK, reqTestCallback);
	regNumberDispatch(g->dispatch, CMD_REQ_LOGIN_TEST, reqLoginTest);
	regNumberDispatch(g->dispatch, CMD_REQ_ParallelTest1, reqParallelTest1);
	regNumberDispatch(g->dispatch, CMD_REQ_ParallelTest2, reqParallelTest2);
	regStringDispatch(g->dispatch, "/reqHttpTest", reqHttpTest);
	regStringDispatch(g->dispatch, "/reqSoTest", reqSoTest);
	regStringDispatch(g->dispatch, "/reqHttpUploadFile", reqHttpUploadFile);
	regStringDispatch(g->dispatch, "/reqTestExecQueue", reqTestExecQueue);
	regStringDispatch(g->dispatch, "/reqClearExecQueue", reqClearExecQueue);

	StackCoSche_function(g->default_task_thread->sche, run, NULL, NULL);
	return 0;
}
