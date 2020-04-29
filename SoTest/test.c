#include "../global.h"
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

__declspec_dllexport int init(int argc, char** argv) {
	regStringDispatch("/reqSoTest", reqSoTest);
	return 1;
}