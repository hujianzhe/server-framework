#include "../global.h"
#include "mq_cmd.h"
#include "mq_cluster.h"
#include "mq_handler.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib,"mq.lib")
#endif

int reqSoTest(UserMsg_t* ctrl) {
	HttpFrame_t* httpframe = ctrl->httpframe;
	printf("module recv http browser ... %s\n", httpframe->query);
	free(httpframeReset(httpframe));

	const char test_data[] = "C so/dll server say hello world, yes ~.~";
	int reply_len;
	char* reply = strFormat(&reply_len,
		"HTTP/1.1 %u %s\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Connection: close\r\n"
		"Content-Length:%u\r\n"
		"\r\n"
		"%s",
		200, httpframeStatusDesc(200), sizeof(test_data) - 1, test_data
	);
	if (!reply) {
		return 0;
	}
	channelShardSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT);
	reactorCommitCmd(NULL, &ctrl->channel->_.stream_sendfincmd);
	free(reply);
	return 0;
}

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int init(int argc, char** argv) {
	initClusterTable();

	set_g_DefaultDispatchCallback(unknowRequest);
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
	regStringDispatch("/reqSoTest", reqSoTest);
	return 1;
}

__declspec_dllexport void destroy(void) {
	freeClusterTable();
}

#ifdef __cplusplus
}
#endif