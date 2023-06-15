#include "../BootServer/global.h"
#include "cmd.h"
#include "test_handler.h"

void reqTestCallback(TaskThread_t* thrd, UserMsg_t* ctrl) {
	char test_data[] = "your callback is from server ^.^";
	InnerMsg_t msg;
	makeInnerMsgRpcResp(&msg, ctrl->rpcid, 0, test_data, sizeof(test_data));
	channelbaseSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
}

void reqTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	char test_data[] = "this text is from server ^.^";
	InnerMsg_t msg;

	printf("say hello world !!! %s\n", (char*)ctrl->data);

	makeInnerMsg(&msg, CMD_NOTIFY_TEST, NULL, 0);
	channelbaseSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);

	makeInnerMsgRpcResp(&msg, ctrl->rpcid, 0, test_data, sizeof(test_data));
	channelbaseSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
}

void reqHttpTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	HttpFrame_t* httpframe = (HttpFrame_t*)ctrl->param.value;
	printf("recv http browser ... %s\n", httpframe->query);

	const char test_data[] = "C server say hello world, yes ~.~";
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
		return;
	}
	channelbaseSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT, NULL, 0);
	channelbaseSendFin(ctrl->channel);
	free(reply);
	return;
}

void reqSoTest(TaskThread_t* thrd, UserMsg_t* ctrl) {
	HttpFrame_t* httpframe = (HttpFrame_t*)ctrl->param.value;
	printf("module recv http browser ... %s\n", httpframe->query);

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
		return;
	}
	channelbaseSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT, NULL, 0);
	channelbaseSendFin(ctrl->channel);
	free(reply);
}

static SerialExecQueue_t* s_dq = NULL;

void reqTestExecQueue(TaskThread_t* thrd, UserMsg_t* ctrl) {
	char* reply;
	int reply_len;
	long long now_msec;
	const char test_data[] = "end test exec queue";

	if (!s_dq) {
		s_dq = (SerialExecQueue_t*)malloc(sizeof(*s_dq));
		if (!s_dq) {
			return;
		}
		SerialExecQueue_init(s_dq);
	}
	if (!SerialExecQueue_check_exec(s_dq, &ctrl->serial)) {
		return;
	}
	puts("start test exec queue");
	now_msec = gmtimeMillisecond();
	StackCoSche_sleep_util(thrd->sche, now_msec + 5000);
	StackCoSche_yield(thrd->sche);

	puts(test_data);
	reply = strFormat(&reply_len,
		"HTTP/1.1 %u %s\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Connection: close\r\n"
		"Content-Length:%u\r\n"
		"\r\n"
		"%s",
		200, httpframeStatusDesc(200), sizeof(test_data) - 1, test_data
	);
	if (!reply) {
		return;
	}
	channelbaseSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT, NULL, 0);
	channelbaseSendFin(ctrl->channel);
	free(reply);
}

void reqClearExecQueue(TaskThread_t* thrd, UserMsg_t* ctrl) {
	char* reply;
	int reply_len;
	const char test_data[] = "clear test exec queue";

	if (s_dq) {
		SerialExecQueue_clear(s_dq, freeUserMsgSerial);
		free(s_dq);
		s_dq = NULL;
	}

	puts(test_data);
	reply = strFormat(&reply_len,
		"HTTP/1.1 %u %s\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Connection: close\r\n"
		"Content-Length:%u\r\n"
		"\r\n"
		"%s",
		200, httpframeStatusDesc(200), sizeof(test_data) - 1, test_data
	);
	if (!reply) {
		return;
	}
	channelbaseSend(ctrl->channel, reply, reply_len, NETPACKET_FRAGMENT, NULL, 0);
	channelbaseSendFin(ctrl->channel);
	free(reply);
}

void reqParallelTest1(TaskThread_t* thrd, UserMsg_t* ctrl) {
	const char reply[] = "reqParallelTest1";

	printf("%s hello world !!! %s\n", __FUNCTION__, (char*)ctrl->data);

	InnerMsg_t msg;
	makeInnerMsgRpcResp(&msg, ctrl->rpcid, 0, reply, sizeof(reply));
	channelbaseSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
}

void reqParallelTest2(TaskThread_t* thrd, UserMsg_t* ctrl) {
	const char reply[] = "reqParallelTest2";

	printf("say hello world !!! %s\n", (char*)ctrl->data);

	InnerMsg_t msg;
	makeInnerMsgRpcResp(&msg, ctrl->rpcid, 0, reply, sizeof(reply));
	channelbaseSendv(ctrl->channel, msg.iov, sizeof(msg.iov) / sizeof(msg.iov[0]), NETPACKET_FRAGMENT, NULL, 0);
}

void reqHttpUploadFile(TaskThread_t* thrd, UserMsg_t* ctrl) {
	HttpFrame_t* httpframe = (HttpFrame_t*)ctrl->param.value;
	ListNode_t* cur, *next;
	char* reply;
	int replylen;
	const char* str_result;
	const char basepath[] = "/data/";
	for (cur = httpframe->multipart_form_datalist.head; cur; cur = next) {
		const char* s, *e;
		char* path;
		FD_t fd;
		int wrbytes;
		HttpMultipartFormData_t* form_data = pod_container_of(cur, HttpMultipartFormData_t, listnode);
		next = cur->next;

		s = httpframeGetHeader(&form_data->headers, "Content-Disposition");
		if (!s)
			continue;
		s = strstr(s, "filename=\"");
		if (!s)
			continue;
		s += sizeof("filename=\"") - 1;
		e = strchr(s, '"');
		if (!e)
			continue;
		path = (char*)malloc(sizeof(basepath) - 1 + e - s);
		if (!path)
			break;
		memmove(path, basepath, sizeof(basepath) - 1);
		memmove(path + sizeof(basepath) - 1, s, e - s);
		path[sizeof(basepath) - 1 + e - s] = 0;

		fd = fdOpen(path, FILE_CREAT_BIT | FILE_WRITE_BIT | FILE_APPEND_BIT);
		if (fd == INVALID_FD_HANDLE) {
			logErr(ptrBSG()->log, "%s open failure", path);
			free(path);
			break;
		}
		logInfo(ptrBSG()->log, "%s open and ready write append %u bytes...", path, form_data->datalen);
		
		wrbytes = fdWrite(fd, form_data->data, form_data->datalen);
		if (wrbytes != form_data->datalen) {
			logErr(ptrBSG()->log, "%s write error", path);
			free(path);
			fdClose(fd);
			break;
		}

		fdClose(fd);
		logInfo(ptrBSG()->log, "%s write %u bytes...", path, wrbytes);
		free(path);
	}
	if (cur) {
		str_result = "UPLOAD ERROR";
	}
	else {
		str_result = "UPLOAD SUCCESS";
	}
	reply = strFormat(&replylen, HTTP_SIMPLE_RESP_FMT, HTTP_SIMPLE_RESP_VALUE(200, str_result, strlen(str_result)));
	if (!reply) {
		return;
	}
	channelbaseSend(ctrl->channel, reply, replylen, NETPACKET_FRAGMENT, NULL, 0);
	channelbaseSendFin(ctrl->channel);
	free(reply);
}
