#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

namespace czrpc
{
namespace base
{
class thread_pool 
{
public:
    explicit thread_pool() : is_stop_threadpool_(false) {}

    ~thread_pool()
    {
        stop();
    }

    void init_thread_num(std::size_t num)
    {
        for (size_t i = 0; i < num; ++i)
        {
            thread_vec_.emplace_back([this] { run_task(); });
        }
    }

    template<typename Function, typename... Args>
    auto add_task(Function&& func, Args&&... args) -> std::future<typename std::result_of<Function(Args...)>::type>
    {
        if (is_stop_threadpool_)
        {
            throw std::runtime_error("Add task on stopped thread pool");
        }

        using return_type = typename std::result_of<Function(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>( std::bind(std::forward<Function>(func), std::forward<Args>(args)...));
        std::future<return_type> res = task->get_future();

        std::unique_lock<std::mutex> lock(mutex_);
        task_queue_.emplace([task](){ (*task)(); });
        lock.unlock();

        cond_.notify_one();
        return res;
    }

    void stop()
    {
        std::call_once(call_flag_, [this]{ stop_impl(); });
    }

    std::size_t size()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return task_queue_.size();
    }

private:
    void run_task()
    {
        while (true)
        {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_.wait(lock, [this]{ return is_stop_threadpool_ || !task_queue_.empty(); });
                if (is_stop_threadpool_ && task_queue_.empty())
                {
                    return;
                }
                task = std::move(task_queue_.front());
                task_queue_.pop();
            }

            task();
        }
    }

    void stop_impl()
    {
        is_stop_threadpool_ = true;
        cond_.notify_all();
        for(auto& th : thread_vec_)
        {
            if (th.joinable())
            {
                th.join();
            }
        }
    }

private:
    std::vector<std::thread> thread_vec_;
    std::queue<std::function<void()>> task_queue_;

    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> is_stop_threadpool_;
    std::once_flag call_flag_;
};

}
}
