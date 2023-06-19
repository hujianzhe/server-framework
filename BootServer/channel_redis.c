#include "global.h"
#include "channel_redis.h"

typedef struct ChannelUserDataRedisClient_t {
	ChannelUserData_t _;
	FnChannelRedisOnSubscribe_t on_subscribe;
	RedisReplyReader_t* reader;
	char* ping_cmd;
	int ping_cmd_len;
	DynArr_t(int) rpc_ids;
} ChannelUserDataRedisClient_t;

static ChannelUserData_t* init_channel_user_data_redis_cli(ChannelUserDataRedisClient_t* ud, struct StackCoSche_t* sche) {
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
	ud->on_subscribe = NULL;
	return initChannelUserData(&ud->_, sche);
}

/********************************************************************/

static void free_user_msg(UserMsg_t* msg) {
	RedisReply_t* reply = (RedisReply_t*)msg->param.value;
	RedisReply_free(reply);
}

static int redis_cli_on_read(ChannelBase_t* channel, unsigned char* buf, unsigned int len, long long timestamp_msec, const struct sockaddr* from_addr, socklen_t addrlen) {
	ChannelUserDataRedisClient_t* ud = (ChannelUserDataRedisClient_t*)channelUserData(channel);
	RedisReplyReader_feed(ud->reader, (const char*)buf, len);
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
				RedisReply_free(reply);
				continue;
			}
			if (0 == strncmp("message", reply->element[0]->str, reply->element[0]->len)) {
				//std::string channel(reply->element[1]->str, reply->element[1]->len);
				//reply->element[2]->str, reply->element[2]->len
				if (reply->elements < 3) {
					continue;
				}
				if (!ud->on_subscribe) {
					continue;
				}
				message = newUserMsg(0);
				if (!message) {
					return -1;
				}
				message->on_free = free_user_msg;
				message->channel = channel;
				message->param.value = reply;
				ud->on_subscribe(channel, message, reply);
				continue;
			}
		}

		if (dynarrIsEmpty(&ud->rpc_ids)) {
			RedisReply_free(reply);
			continue;
		}
		rpc_id = ud->rpc_ids.buf[0];
		dynarrRemoveIdx(&ud->rpc_ids, 0);
		if (0 == rpc_id) {
			RedisReply_free(reply);
			continue;
		}

		message = newUserMsg(0);
		if (!message) {
			RedisReply_free(reply);
			return -1;
		}
		message->on_free = free_user_msg;
		message->channel = channel;
		message->param.value = reply;
		message->rpcid = rpc_id;
		StackCoSche_resume_block_by_id(ud->_.sche, rpc_id, STACK_CO_STATUS_FINISH, message, (void(*)(void*))freeUserMsg);
	}
	return len;
}

static int redis_cli_on_pre_send(ChannelBase_t* channel, NetPacket_t* packet, long long timestamp_msec) {
	int rpc_id;
	int ret_ok;
	ChannelUserDataRedisClient_t* ud = (ChannelUserDataRedisClient_t*)channelUserData(channel);
	if (packet->bodylen < sizeof(int)) {
		channel->valid = 0;
		return 0;
	}

	rpc_id = *(int*)(packet->buf + packet->bodylen - sizeof(int));
	dynarrInsert(&ud->rpc_ids, ud->rpc_ids.len, rpc_id, ret_ok);
	if (!ret_ok) {
		return 0;
	}
	packet->bodylen -= sizeof(int);
	return 1;
}

static void redis_cli_on_heartbeat(ChannelBase_t* channel, int heartbeat_times) {
	ChannelUserDataRedisClient_t* ud = (ChannelUserDataRedisClient_t*)channelUserData(channel);
	int rpc_id = 0;
	Iobuf_t iovs[2] = {
		iobufStaticInit(ud->ping_cmd, (size_t)ud->ping_cmd_len),
		iobufStaticInit(&rpc_id, sizeof(rpc_id))
	};
	channelbaseSendv(channel, iovs, 2, NETPACKET_NO_ACK_FRAGMENT, NULL, 0);
}

static void redis_cli_on_free(ChannelBase_t* channel) {
	ChannelUserDataRedisClient_t* ud = (ChannelUserDataRedisClient_t*)channelUserData(channel);
	RedisReplyReader_free(ud->reader);
	RedisCommand_free(ud->ping_cmd);
	dynarrFreeMemory(&ud->rpc_ids);
	free(ud);
}

static ChannelBaseProc_t s_redis_cli_proc = {
	NULL,
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

ChannelBase_t* openChannelRedisClient(const char* ip, unsigned short port, FnChannelRedisOnSubscribe_t on_subscribe, struct StackCoSche_t* sche) {
	ChannelBase_t* c;
	ChannelUserDataRedisClient_t* ud;
	Sockaddr_t addr;
	int domain = ipstrFamily(ip);

	if (!sockaddrEncode(&addr.sa, domain, ip, port)) {
		return NULL;
	}
	ud = (ChannelUserDataRedisClient_t*)malloc(sizeof(ChannelUserDataRedisClient_t));
	if (!ud) {
		return NULL;
	}
	if (!init_channel_user_data_redis_cli(ud, sche)) {
		free(ud);
		return NULL;
	}
	c = channelbaseOpen(CHANNEL_FLAG_CLIENT, &s_redis_cli_proc, INVALID_FD_HANDLE, domain, SOCK_STREAM, 0);
	if (!c) {
		free(ud);
		return NULL;
	}
	channelbaseSetOperatorSockaddr(c, &addr.sa, sockaddrLength(domain));
	ud->on_subscribe = on_subscribe;
	channelSetUserData(c, &ud->_);
	c->heartbeat_timeout_sec = 10;
	c->heartbeat_maxtimes = 3;
	return c;
}

void channelRedisClientAsyncSendCommand(ChannelBase_t* channel, int rpc_id, const char* format, ...) {
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
	channelbaseSendv(channel, iovs, 2, NETPACKET_FRAGMENT, NULL, 0);
	RedisCommand_free(cmd);
}

#ifdef __cplusplus
}
#endif
