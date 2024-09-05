#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"
#include <stdio.h>

static void reflect_websocket_on_recv(NetChannel_t* channel, unsigned char* bodyptr, size_t bodylen, const struct sockaddr* saddr, socklen_t addrlen) {
	printf("reflect_websocket_on_recv: %zu bytes\n", bodylen);
	NetChannel_send(channel, bodyptr, bodylen, NETPACKET_FRAGMENT, saddr, addrlen);
}

static int simply_dgram_on_read(NetChannel_t* channel, unsigned char* buf, unsigned int len, long long timestamp_msec, const struct sockaddr* from_addr, socklen_t addrlen) {
	IPString_t ip;
	unsigned short port;
	if (!sockaddrDecode(from_addr, ip, &port)) {
		return len;
	}
	printf("reflect_udp_on_recv from %s:%hu, %u bytes, %s\n", ip, port, len, (char*)buf);
	NetChannel_send(channel, buf, len, 0, from_addr, addrlen);
	return len;
}

static NetChannelProc_t s_simply_udp_proc = {
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
	NetChannel_t* c;
	Sockaddr_t saddr;
	socklen_t saddrlen = sockaddrEncode(&saddr.sa, AF_INET, NULL, 45678);
	if (saddrlen <= 0) {
		return;
	}
	fd = socket(saddr.sa.sa_family, SOCK_DGRAM, 0);
	if (INVALID_FD_HANDLE == fd) {
		return;
	}
	if (!socketBindAndReuse(fd, &saddr.sa, saddrlen)) {
		socketClose(fd);
		return;
	}
	c = NetChannel_open_with_fd(0, &s_simply_udp_proc, fd, saddr.sa.sa_family, 0);
	if (!c) {
		socketClose(fd);
		return;
	}
	NetChannel_reg(selectNetReactor(), c);
	NetChannel_close_ref(c);
}

static void net_dispatch(TaskThread_t* thrd, DispatchNetMsg_t* net_msg) {
	net_msg->callback(thrd, net_msg);
}

void run(struct StackCoSche_t* sche, StackCoAsyncParam_t* param) {
	TaskThread_t* thrd = currentTaskThread();
	int i;
	// listen extra port
	for (i = 0; i < ptrBSG()->conf->listen_options_cnt; ++i) {
		const ConfigListenOption_t* option = ptrBSG()->conf->listen_options + i;
		NetChannel_t* c;
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
		NetChannel_reg(acceptNetReactor(), c);
		NetChannel_close_ref(c);
	}
	// listen normal udp
	test_simply_udp_server(45678);
}

int init(void) {
	// register dispatch
	regNumberDispatch(ptrBSG()->dispatch, CMD_REQ_TEST, reqTest);
	regNumberDispatch(ptrBSG()->dispatch, CMD_REQ_TEST_CALLBACK, reqTestCallback);
	regNumberDispatch(ptrBSG()->dispatch, CMD_REQ_LOGIN_TEST, reqLoginTest);
	regNumberDispatch(ptrBSG()->dispatch, CMD_REQ_ParallelTest1, reqParallelTest1);
	regNumberDispatch(ptrBSG()->dispatch, CMD_REQ_ParallelTest2, reqParallelTest2);
	regNumberDispatch(ptrBSG()->dispatch, CMD_REQ_ECHO, reqEcho);
	regStringDispatch(ptrBSG()->dispatch, "/reqHttpTest", reqHttpTest);
	regStringDispatch(ptrBSG()->dispatch, "/reqSoTest", reqSoTest);
	regStringDispatch(ptrBSG()->dispatch, "/reqHttpUploadFile", reqHttpUploadFile);
	regStringDispatch(ptrBSG()->dispatch, "/reqTestExecQueue", reqTestExecQueue);

	ptrBSG()->default_task_thread->net_dispatch = net_dispatch;
	StackCoSche_function(ptrBSG()->default_task_thread->sche, run, NULL);
	return 0;
}
