#include "global.h"
#include "channel_redis.h"

typedef struct ChannelUserDataRedisClient_t {
	ChannelUserData_t _;
	RedisReplyReader_t* reader;
	char* ping_cmd;
	int ping_cmd_len;
	DynArr_t(int) rpc_ids;
} ChannelUserDataRedisClient_t;

static ChannelUserData_t* init_channel_user_data_redis_cli(ChannelUserDataRedisClient_t* ud, struct DataQueue_t* dq) {
	ud->ping_cmd_len = RedisCommand_format(&ud->ping_cmd, "PING");
	if (ud->ping_cmd_len < 0) {
		ud->ping_cmd = NULL;
		return NULL;
	}
	ud->reader = RedisReplyReader_create();
	if (!ud->reader) {
		RedisCommand_free(ud->ping_cmd);
		ud->ping_cmd = NULL;
		return NULL;
	}
	dynarrInitZero(&ud->rpc_ids);
	return initChannelUserData(&ud->_, dq);
}

/********************************************************************/

static void free_user_msg(UserMsg_t* msg) {
	RedisReply_t* reply = (RedisReply_t*)msg->param.value;
	RedisReply_free(reply);
	free(msg);
}

static int redis_cli_on_read(ChannelBase_t* channel, unsigned char* buf, unsigned int len, long long timestamp_msec, const struct sockaddr* from_addr) {
	ChannelUserDataRedisClient_t* ud = (ChannelUserDataRedisClient_t*)channelUserData(channel);
	RedisReplyReader_feed(ud->reader, buf, len);
	while (1) {
		int ret, rpc_id;
		RedisReply_t* reply;
		UserMsg_t* message;

		ret = RedisReplyReader_pop_reply(ud->reader, &reply);
		if (ret != REDIS_OK) {
			return -1;
		}
		if (!reply) {
			break;
		}

		if (REDIS_REPLY_ARRAY == reply->type) {
			if (reply->elements <= 0) {
				continue;
			}
			if (0 == strncmp("message", reply->element[0]->str, reply->element[0]->len)) {
				//std::string channel(reply->element[1]->str, reply->element[1]->len);
				//reply->element[2]->str, reply->element[2]->len
				message = newUserMsg(0);
				if (!message) {
					return -1;
				}
				message->on_free = free_user_msg;
				message->channel = channel;
				message->param.value = reply;
				dataqueuePush(ud->_.dq, &message->internal._);
				continue;
			}
		}

		if (dynarrIsEmpty(&ud->rpc_ids)) {
			continue;
		}
		rpc_id = ud->rpc_ids.buf[0];
		dynarrRemoveIdx(&ud->rpc_ids, 0);
		if (0 == rpc_id) {
			continue;
		}

		message = newUserMsg(0);
		if (!message) {
			return -1;
		}
		message->on_free = free_user_msg;
		message->channel = channel;
		message->param.value = reply;
		message->rpc_status = RPC_STATUS_RESP;
		message->rpcid = rpc_id;
		dataqueuePush(ud->_.dq, &message->internal._);
	}
	return len;
}

static int redis_cli_on_pre_send(ChannelBase_t* channel, NetPacket_t* packet, long long timestamp_msec) {
	int rpc_id;
	int ret_ok;
	ChannelUserDataRedisClient_t* ud = (ChannelUserDataRedisClient_t*)channelUserData(channel);

	rpc_id = *(int*)(packet->buf + packet->bodylen - sizeof(int));
	dynarrInsert(&ud->rpc_ids, ud->rpc_ids.len, rpc_id, ret_ok);
	if (!ret_ok) {
		return 0;
	}
	packet->bodylen -= sizeof(int);
	return 1;
}

static void redis_cli_on_heartbeat(ChannelBase_t* channel, int heartbeat_times) {
	int ret_ok;
	ChannelUserDataRedisClient_t* ud = (ChannelUserDataRedisClient_t*)channelUserData(channel);

	dynarrInsert(&ud->rpc_ids, ud->rpc_ids.len, 0, ret_ok);
	if (!ret_ok) {
		return;
	}
	channelbaseSend(channel, ud->ping_cmd, ud->ping_cmd_len, NETPACKET_NO_ACK_FRAGMENT);
}

static void redis_cli_on_free(ChannelBase_t* channel) {
	ChannelUserDataRedisClient_t* ud = (ChannelUserDataRedisClient_t*)channelUserData(channel);
	RedisReplyReader_free(ud->reader);
	RedisCommand_free(ud->ping_cmd);
	dynarrFreeMemory(&ud->rpc_ids);
}

static ChannelBaseProc_t s_redis_cli_proc = {
	defaultChannelOnReg,
	NULL,
	redis_cli_on_read,
	NULL,
	redis_cli_on_pre_send,
	redis_cli_on_heartbeat,
	defaultChannelOnDetach,
	redis_cli_on_free
};

/********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

ChannelBase_t* openChannelRedisClient(ReactorObject_t* o, const struct sockaddr* addr, struct DataQueue_t* dq) {
	ChannelUserDataRedisClient_t* ud;
	ChannelBase_t* c = channelbaseOpen(sizeof(ChannelBase_t) + sizeof(ChannelUserDataRedisClient_t), CHANNEL_FLAG_CLIENT, o, addr);
	if (!c) {
		return NULL;
	}
	ud = (ChannelUserDataRedisClient_t*)(c + 1);
	if (!init_channel_user_data_redis_cli(ud, dq)) {
		reactorCommitCmd(NULL, &c->freecmd);
		return NULL;
	}
	channelSetUserData(c, &ud->_);
	c->proc = &s_redis_cli_proc;
	c->heartbeat_timeout_sec = 10;
	c->heartbeat_maxtimes = 3;
	return c;
}

void channelSendRedisCommand(ChannelBase_t* channel, int rpc_id, const char* format, ...) {
	char* cmd;
	int cmdlen;
	va_list ap;
	Iobuf_t iovs[2];

	va_start(ap, format);
	cmdlen = RedisCommand_vformat(&cmd, format, ap);
	va_end(ap);
	if (cmdlen <= 0) {
		return;
	}
	iobufPtr(iovs + 0) = cmd;
	iobufLen(iovs + 0) = cmdlen;
	iobufPtr(iovs + 1) = (char*)&rpc_id;
	iobufLen(iovs + 1) = sizeof(rpc_id);
	channelbaseSendv(channel, iovs, 2, NETPACKET_FRAGMENT);
	RedisCommand_free(cmd);
}

#ifdef __cplusplus
}
#endif
