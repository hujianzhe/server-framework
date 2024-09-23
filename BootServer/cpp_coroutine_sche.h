#ifndef BOOT_SERVER_CPP_COROUTINE_SCHE_H
#define	BOOT_SERVER_CPP_COROUTINE_SCHE_H

#include "task_thread.h"
#include "net_channel_proc_imp.h"
#include "dispatch_msg.h"
#include "util/cpp_inc/coroutine_default_sche.h"
#include "util/cpp_inc/std_any_pointer_guard.h"
#include <cassert>
#include <memory>

class TaskThreadCppCoroutine : public TaskThread_t {
public:
	typedef util::CoroutinePromise<void>(*FnNetCallback)(TaskThread_t*, DispatchNetMsg_t*);
	typedef util::CoroutinePromise<void>(*FnNetDetach)(TaskThread_t*, NetChannel_t*);

	static TaskThread_t* newInstance() {
		auto t = new TaskThreadCppCoroutine();
		if (!saveTaskThread(t)) {
			delete t;
			return nullptr;
		}
		return t;
	}

	FnNetCallback net_dispatch;
	FnNetDetach net_detach;

private:
	util::CoroutineDefaultSche m_sche;

	TaskThreadCppCoroutine() {
		net_dispatch = [](TaskThread_t* t, DispatchNetMsg_t* net_msg)->util::CoroutinePromise<void> {
			FnNetCallback fn = (FnNetCallback)net_msg->callback;
			co_await fn(t, net_msg);
			co_return;
		};
		net_detach = [](TaskThread_t*, NetChannel_t*)->util::CoroutinePromise<void> { co_return; };
		sche = &m_sche;
		mt19937Seed(&randmt19937_ctx, time(NULL));
		hook = &TaskThreadCppCoroutine::default_hook;
	}

	static unsigned int entry(void* arg) {
		auto t = ((TaskThreadCppCoroutine*)arg);
		auto& sche = t->m_sche;
		while (!sche.check_exit()) {
			sche.doSche(-1);
		}
		sche.scheDestroy();
		if (t->detached) {
			freeTaskThread(t);
		}
		return 0;
	}
	static void exit(TaskThread_t* t) {
		auto& sche = ((TaskThreadCppCoroutine*)t)->m_sche;
		sche.doExit();
	}
	static void deleter(TaskThread_t* t) {
		delete ((TaskThreadCppCoroutine*)t);
	}

	static constexpr TaskThreadHook_t default_hook = {
		entry,
		exit,
		deleter
	};
};

class NetScheHookCppCoroutine {
private:
	static void on_detach(void* sche_obj, NetChannel_t* channel) {
		auto sche = (util::CoroutineDefaultSche*)sche_obj;
		sche->readyExec([](const std::any& arg)->util::CoroutinePromise<void> {
			auto thrd = (TaskThreadCppCoroutine*)currentTaskThread();
			auto channel = util::StdAnyPointerGuard::transfer_unique_ptr<NetChannel_t>(arg);
			co_await thrd->net_detach(thrd, channel.get());
			co_return;
		}, util::StdAnyPointerGuard::to_any(channel, NetChannel_close_ref));
	}
	static void on_execute_msg(void* sche_obj, DispatchNetMsg_t* msg) {
		auto sche = (util::CoroutineDefaultSche*)sche_obj;
		sche->readyExec([](const std::any& arg)->util::CoroutinePromise<void> {
			auto thrd = (TaskThreadCppCoroutine*)currentTaskThread();
			auto net_msg = util::StdAnyPointerGuard::transfer_unique_ptr<DispatchNetMsg_t>(arg);
			std::unique_ptr<NetChannel_t, void(*)(NetChannel_t*)> ch(NetChannel_add_ref(net_msg->channel), NetChannel_close_ref);
			co_await thrd->net_dispatch(thrd, net_msg.get());
			co_return;
		}, util::StdAnyPointerGuard::to_any(msg, freeDispatchNetMsg));
	}
	static void on_resume_msg(void* sche_obj, DispatchNetMsg_t* msg) {
		auto sche = (util::CoroutineDefaultSche*)sche_obj;
		sche->readyResume(msg->rpcid, util::StdAnyPointerGuard::to_any(msg, freeDispatchNetMsg));
	}
	static void on_resume(void* sche_obj, int id, int canceled) {
		auto sche = (util::CoroutineDefaultSche*)sche_obj;
		if (canceled) {
			sche->readyCancel(id);
		}
		else {
			sche->readyResume(id);
		}
	}

public:
	static constexpr NetScheHook_t hook = {
		on_detach,
		on_execute_msg,
		on_resume_msg,
		on_resume
	};
};

#endif
