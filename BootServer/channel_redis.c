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

static void free_user_msg(DispatchBaseMsg_t* msg) {
	DispatchNetMsg_t* net_msg = pod_container_of(msg, DispatchNetMsg_t, base);
	RedisReply_t* reply = (RedisReply_t*)net_msg->param.value;
	RedisReply_free(reply);
	freeDispatchNetMsg(msg);
}

static int redis_cli_on_read(ChannelBase_t* channel, unsigned char* buf, unsigned int len, long long timestamp_msec, const struct sockaddr* from_addr, socklen_t addrlen) {
	ChannelUserDataRedisClient_t* ud = (ChannelUserDataRedisClient_t*)channelUserData(channel);
	RedisReplyReader_feed(ud->reader, (const char*)buf, len);
	while (1) {
		int ret, rpc_id;
		RedisReply_t* reply;
		DispatchNetMsg_t* message;
		StackCoAsyncParam_t async_param;

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
				message = newDispatchNetMsg(channel, 0, free_user_msg);
				if (!message) {
					return -1;
				}
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

		message = newDispatchNetMsg(channel, 0, free_user_msg);
		if (!message) {
			RedisReply_free(reply);
			return -1;
		}
		message->param.value = reply;
		message->base.rpcid = rpc_id;

		memset(&async_param, 0, sizeof(async_param));
		async_param.value = message;
		async_param.fn_value_free = (void(*)(void*))message->base.on_free;
		StackCoSche_resume_block_by_id(ud->_.sche, rpc_id, STACK_CO_STATUS_FINISH, &async_param);
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
	ChannelBase_t* c = NULL;
	ChannelUserDataRedisClient_t* ud = NULL;
	Sockaddr_t connect_addr;
	socklen_t connect_addrlen;
	int domain = ipstrFamily(ip);

	connect_addrlen = sockaddrEncode(&connect_addr.sa, domain, ip, port);
	if (connect_addrlen <= 0) {
		goto err;
	}
	ud = (ChannelUserDataRedisClient_t*)malloc(sizeof(ChannelUserDataRedisClient_t));
	if (!ud) {
		return NULL;
	}
	if (!init_channel_user_data_redis_cli(ud, sche)) {
		goto err;
	}
	c = channelbaseOpen(CHANNEL_SIDE_CLIENT, &s_redis_cli_proc, domain, SOCK_STREAM, 0);
	if (!c) {
		goto err;
	}
	channelbaseSetOperatorSockaddr(c, &connect_addr.sa, connect_addrlen);
	ud->on_subscribe = on_subscribe;
	channelSetUserData(c, &ud->_);
	c->heartbeat_timeout_sec = 10;
	c->heartbeat_maxtimes = 3;
	return c;
err:
	free(ud);
	channelbaseCloseRef(c);
	return NULL;
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
