#include "AsyncQueue.hpp"

#include <Geode/loader/Log.hpp>

#include <condition_variable>
#include <exception>
#include <mutex>
#include <queue>
#include <thread>

namespace git_editor {

namespace {

class WorkerQueue final {
public:
    WorkerQueue() : m_thread([this]() { this->run(); }) {}

    ~WorkerQueue() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_cv.notify_one();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void post(std::function<void()> job) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_jobs.push(std::move(job));
        }
        m_cv.notify_one();
    }

private:
    void run() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]() {
                    return m_stopping || !m_jobs.empty();
                });
                if (m_stopping && m_jobs.empty()) {
                    return;
                }
                job = std::move(m_jobs.front());
                m_jobs.pop();
            }
            if (job) {
                try {
                    job();
                } catch (std::exception const& e) {
                    geode::log::error("git worker job threw: {}", e.what());
                } catch (...) {
                    geode::log::error("git worker job threw unknown exception");
                }
            }
        }
    }

    std::mutex                        m_mutex;
    std::condition_variable           m_cv;
    std::queue<std::function<void()>> m_jobs;
    bool                              m_stopping = false;
    std::thread                       m_thread;
};

} // namespace

void postToGitWorker(std::function<void()> job) {
    static WorkerQueue queue;
    queue.post(std::move(job));
}

} // namespace git_editor
