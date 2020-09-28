#pragma once
#include <ctype.h>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <unistd.h>
#include <vector>

class ThreadPool {
    std::atomic<bool> time_to_die{false};
    std::mutex qmutex;
    std::condition_variable new_task_or_end;
    std::condition_variable &task_finished;
    std::deque<std::function<void()>> deq;
    std::vector<std::thread> threads;
    int working_threads{0};

public:
    explicit ThreadPool(size_t poolSize, std::condition_variable &task_finished) : task_finished(task_finished) {
        fprintf(stderr, "POOOL %d\n", working_threads);
        for (size_t i = 0; i < poolSize; ++i) {
            threads.emplace_back(&ThreadPool::worker, this);
        }
    }

    ~ThreadPool() {
        time_to_die = true;
        new_task_or_end.notify_all();
        for (auto &thread : threads)
            thread.join();
    }
    void worker() {
        while (true) {
            std::unique_lock<std::mutex> lock(qmutex);
            while (!deq.size() && !time_to_die)
                new_task_or_end.wait(lock);
            if (time_to_die)
                return;
            auto task = deq.front();
            deq.pop_front();
            ++working_threads;
            fprintf(stderr, "Insrece %d\n", working_threads);
            lock.unlock();
            task();
            --working_threads;
            fprintf(stderr, "Desrece %d\n", working_threads);
            task_finished.notify_all();
            lock.lock();
        }
    }

    int get_working_number() {
        std::unique_lock<std::mutex> lock(qmutex);
        return working_threads;
    }

    // pass arguments by value
    template <class Func, class... Args> auto exec(Func func, Args... args) -> std::future<decltype(func(args...))> {
        auto task = std::make_shared<std::packaged_task<decltype(func(args...))()>>(std::bind(func, args...));
        auto future = task->get_future();
        std::unique_lock<std::mutex> lock(qmutex);
        deq.emplace_back([task]() { (*task)(); });
        lock.unlock();
        new_task_or_end.notify_one();
        return future;
    }
};
