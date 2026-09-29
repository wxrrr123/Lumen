#pragma once
#include "../LumenPCH.h"

namespace lumen {
class ThreadPool {
   public:
	template <typename FunctionType, typename... Args>
	static auto submit(FunctionType&& f, Args&&... args);
	static void init();
	static void destroy();

	// LUMEN_SERIAL_TASKS=1: submit() runs each task inline in the caller, so
	// shader/pipeline creation happens in a fixed order (Vulkan-Sim assigns
	// shader IDs and code addresses in creation order).
	static bool serial;

   private:
	static std::atomic_bool done;
	static std::queue<std::function<void()>> work_queue;
	static std::mutex queue_mutex;
	static std::condition_variable cv;
	static std::vector<std::thread> threads;
};

template <typename FunctionType, typename... Args>
auto ThreadPool::submit(FunctionType&& f, Args&&... args) {
	using result_type = std::invoke_result_t<FunctionType, Args...>;
	auto task = std::make_shared<std::packaged_task<result_type()>>(
		std::bind(std::forward<FunctionType>(f), std::forward<Args>(args)...));
	auto result = task->get_future();
	if (serial) {
		(*task)();
		return result;
	}
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		if (done) {
			LUMEN_ERROR("ThreadPool has been terminated");
		}
		work_queue.emplace([task]() { (*task)(); });
	}
	cv.notify_one();
	return result;
}

}  // namespace lumen
