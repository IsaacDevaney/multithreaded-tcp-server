#include "thread_pool.h"

ThreadPool::ThreadPool(std::size_t thread_count) {
    for (std::size_t i = 0; i < thread_count; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(queue_mutex);

                    condition.wait(lock, [this] {
                        return stop || !tasks.empty();
                    });

                    if (stop && tasks.empty()) {
                        return;
                    }

                    task = std::move(tasks.front());
                    tasks.pop();
                }

                task();
            }
        });
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        if (stop) {
            return;
        }

        tasks.push(std::move(task));
    }

    condition.notify_one();
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }

    condition.notify_all();

    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}