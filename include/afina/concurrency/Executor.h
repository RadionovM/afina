#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor;
void perform(Executor *executor);
class Executor {
public:
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    //    friend void perform(Executor *executor);
    Executor(std::size_t low_watermark, std::size_t hight_watermark, std::size_t max_queue_size, std::size_t idle_time)
        : state(State::kRun), low_watermark(low_watermark), hight_watermark(hight_watermark),
          max_queue_size(max_queue_size), idle_time(idle_time), thread_count(0), worked_threads(0) {
        // perform_thread = std::make_unique<std::thread>(perform, this);
        for (std::size_t i = 0; i < low_watermark; ++i) {
            std::thread(&Executor::worker, this).detach();
            ++thread_count;
        }
    }
    void worker() {
        while (true) {
            std::unique_lock<std::mutex> lock(this->mutex);
            auto now = std::chrono::system_clock::now();
            if (thread_count > low_watermark) {
                empty_condition_or_stop.wait_until(lock, now + std::chrono::milliseconds(idle_time),
                                                   [&]() { return !(state == State::kRun && tasks.empty()); });
            } else {
                while (state == State::kRun && tasks.empty()) {
                    empty_condition_or_stop.wait(lock);
                }
            }
            if (tasks.empty() && (thread_count > low_watermark || state != State::kRun)) {
                break;
            }
            auto task = tasks.front();
            tasks.pop_front();
            ++worked_threads;
            lock.unlock();
            task();
            lock.lock();
            --worked_threads;
        }
        std::unique_lock<std::mutex> lock(this->mutex);
        --thread_count;
        if (!thread_count) {
            lock.unlock();
            last_thread.notify_one();
        }
    }

    ~Executor() { Stop(true); }

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false) {
        std::unique_lock<std::mutex> lock(this->mutex);
        state = State::kStopping;
        empty_condition_or_stop.notify_all();

        if (await) {
            while (thread_count > 0) {
                last_thread.wait(lock);
            }
        }

        state = State::kStopped;
    }

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->mutex);
        if (state != State::kRun) {
            return false;
        }
        if (thread_count >= hight_watermark && tasks.size() >= max_queue_size) {
            return false;
        }
        tasks.push_back(exec);
        if (!tasks.empty() && worked_threads == thread_count && thread_count < hight_watermark) {
            std::thread(&Executor::worker, this).detach();
            ++thread_count;
        }
        // Enqueue new task
        empty_condition_or_stop.notify_all(); //поменять
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition_or_stop;
    std::condition_variable last_thread;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    std::unique_ptr<std::thread> perform_thread;
    State state;
    std::size_t low_watermark;
    std::size_t hight_watermark;
    std::size_t max_queue_size;
    std::size_t idle_time;
    std::size_t thread_count;
    std::size_t worked_threads;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
