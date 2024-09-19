#ifndef BOOT_SERVER_CPP_COROUTINE_SCHE_H
#define	BOOT_SERVER_CPP_COROUTINE_SCHE_H

#include "task_thread.h"
#include "net_channel_proc_imp.h"
#include "dispatch_msg.h"
#include "util/cpp_inc/coroutine_default_sche.h"
#include "util/cpp_inc/std_any_pointer_guard.h"
#include <cassert>
#include <memory>

typedef util::CoroutinePromise<void>(*CppCoroutineDispatchNetCallback)(TaskThread_t*, DispatchNetMsg_t*);

class TaskThreadCppCoroutine : public TaskThread_t {
public:
	static TaskThread_t* newInstance() {
		auto t = new TaskThreadCppCoroutine();
		if (!saveTaskThread(t)) {
			delete t;
			return nullptr;
		}
		return t;
	}

	CppCoroutineDispatchNetCallback net_dispatch;

private:
	util::CoroutineDefaultSche m_sche;

	TaskThreadCppCoroutine() {
		net_dispatch = nullptr;
		sche = &m_sche;
		mt19937Seed(&randmt19937_ctx, time(NULL));
		hook = &TaskThreadCppCoroutine::default_hook;
	}

	static unsigned int entry(void* arg) {
		auto& sche = ((TaskThreadCppCoroutine*)arg)->m_sche;
		while (!sche.check_exit()) {
			sche.doSche(-1);
		}
		sche.scheDestroy();
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
			auto channel = std::any_cast<NetChannel_t*>(arg);
			std::unique_ptr<NetChannel_t, void(*)(NetChannel_t*)> ch(channel, NetChannel_close_ref);
			NetSession_t* session = channel->session;
			if (!session) {
				co_return;
			}
			channel->session = NULL;
			session->channel = NULL;
			auto fn = (util::CoroutineDefaultSche::EntryFuncPtr)session->on_disconnect_fn_ptr;
			if (fn) {
				co_await fn(session);
			}
			co_return;
		}, channel);
	}
	static void on_execute_msg(void* sche_obj, DispatchNetMsg_t* msg) {
		auto sche = (util::CoroutineDefaultSche*)sche_obj;
		sche->readyExec([](const std::any& arg)->util::CoroutinePromise<void> {
			auto thrd = (TaskThreadCppCoroutine*)currentTaskThread();
			auto net_msg = util::StdAnyPointerGuard::transfer_unique_ptr<DispatchNetMsg_t>(arg);
		#ifndef NDEBUG
			assert(thrd->net_dispatch);
		#endif
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
