#include "../BootServer/config.h"
#include "../BootServer/global.h"
#include "../BootServer/cpp_coroutine_sche.h"
#include "test_handler.h"
#include <iostream>
#include <memory>

static util::CoroutinePromise<void> run(const std::any& param) {
	auto sc = util::CoroutineDefaultSche::get();
	TaskThread_t* thrd = currentTaskThread();
	// listen extra port
	for (int i = 0; i < ptrBSG()->conf->listen_options_cnt; ++i) {
		const ConfigListenOption_t* option = ptrBSG()->conf->listen_options + i;
		std::unique_ptr<NetChannel_t, void(*)(NetChannel_t*)> c(nullptr, NetChannel_close_ref);
		if (!strcmp(option->protocol, "http")) {
			c.reset(openNetListenerHttp(option->ip, option->port, thrd->sche));
		}
		else {
			continue;
		}
		if (!c) {
			logErr(ptrBSG()->log, "listen failure, ip:%s, port:%u ......", option->ip, option->port);
			co_return;
		}
		NetChannel_reg(acceptNetReactor(), c.get());
	}
	// connect redis
	/*
	std::unique_ptr<NetChannel_t, void(*)(NetChannel_t*)> c(nullptr, NetChannel_close_ref);
	c.reset(openNetChannelRedisClient("10.1.1.186", 6379, [](NetChannel_t* ch, DispatchNetMsg_t* msg, RedisReply_t* reply)-> void {
		std::string channel_name(reply->element[1]->str, reply->element[1]->len);
		std::string data(reply->element[2]->str, reply->element[2]->len);
		std::cout << "channel[" << channel_name << "] recv: " << data << std::endl;
		freeDispatchNetMsg(msg);
	}, thrd->sche));
	if (!c) {
		logErr(ptrBSG()->log, "connect redis failure, ip:%s, port:%u ......", "10.1.1.186", 6379);
		co_return;
	}

	c->on_syn_ack = defaultNetChannelOnSynAck;
	c->connect_timeout_sec = 5;
	auto awaiter1 = sc->blockPointTimeout(5000);
	NetChannel_get_userdata(c.get())->rpc_id_syn_ack = awaiter1.id();
	NetChannel_reg(selectNetReactor(), c.get());
	co_await awaiter1;
	if (awaiter1.status() != util::CoroutineAwaiter::STATUS_FINISH) {
		std::cout << "connect redis timeout" << std::endl;
		co_return;
	}
	std::cout << "connect redis success" << std::endl;

	auto awaiter2 = sc->blockPointTimeout(1000);
	sendRedisCmdByNetChannel(c.get(), awaiter2.id(), "AUTH %s", "123456");
	auto resume_msg2 = util::StdAnyPointerGuard::transfer_unique_ptr<DispatchNetMsg_t>(co_await awaiter2);
	RedisReply_t* redis_reply2 = (RedisReply_t*)resume_msg2->param.value;
	if (REDIS_REPLY_ERROR == redis_reply2->type) {
		std::cout << std::string(redis_reply2->str, redis_reply2->len) << std::endl;
		NetChannel_send_fin(c.get());
		co_return;
	}

	auto awaiter3 = sc->blockPointTimeout(1000);
	sendRedisCmdByNetChannel(c.get(), awaiter3.id(), "SUBSCRIBE %s", "cnm");
	auto resume_msg3 = util::StdAnyPointerGuard::transfer_unique_ptr<DispatchNetMsg_t>(co_await awaiter3);
	RedisReply_t* redis_reply3 = (RedisReply_t*)resume_msg3->param.value;
	if (REDIS_REPLY_ERROR == redis_reply3->type) {
		std::cout << std::string(redis_reply3->str, redis_reply3->len) << std::endl;
		NetChannel_send_fin(c.get());
		co_return;
	}
	*/
	co_return;
}

int init(void) {
	auto sc = (util::CoroutineDefaultSche*)ptrBSG()->default_task_thread->sche;
	// register dispatch
	TestHandler::reg_dispatch(ptrBSG()->dispatch);

	sc->readyExec(run);
	return 0;
}
