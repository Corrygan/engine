#include "JobSystem.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <algorithm>

namespace jobs {

struct Job {
    std::function<void()> fn;
    WaitGroup*            wg = nullptr;
};

struct JobSystem::Impl {
    std::vector<std::thread> workers;
    std::deque<Job>          queue;
    std::mutex               mtx;
    std::condition_variable  cv;
    bool                     stop = false;

    std::mutex                          mainMtx;
    std::vector<std::function<void()>>  mainTasks;

    void push(Job j) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            queue.push_back(std::move(j));
        }
        cv.notify_one();
    }

    bool tryPop(Job& out) {
        std::lock_guard<std::mutex> lk(mtx);
        if (queue.empty()) return false;
        out = std::move(queue.front());
        queue.pop_front();
        return true;
    }

    static void run(Job& j) {
        j.fn();
        if (j.wg) j.wg->remaining.fetch_sub(1, std::memory_order_acq_rel);
    }

    void workerLoop() {
        for (;;) {
            Job j;
            {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&] { return stop || !queue.empty(); });
                if (stop && queue.empty()) return;
                j = std::move(queue.front());
                queue.pop_front();
            }
            run(j);
        }
    }
};

JobSystem& JobSystem::Get() {
    static JobSystem s;
    return s;
}

JobSystem::~JobSystem() {
    Shutdown();
}

void JobSystem::Initialize(unsigned workerCount) {
    if (m_impl) return;
    m_impl = new Impl();

    unsigned hw = std::thread::hardware_concurrency();
    unsigned n  = workerCount ? workerCount : (hw > 1 ? hw - 1 : 1);

    m_impl->workers.reserve(n);
    for (unsigned i = 0; i < n; ++i)
        m_impl->workers.emplace_back([this] { m_impl->workerLoop(); });
}

void JobSystem::Shutdown() {
    if (!m_impl) return;
    {
        std::lock_guard<std::mutex> lk(m_impl->mtx);
        m_impl->stop = true;
    }
    m_impl->cv.notify_all();
    for (auto& t : m_impl->workers)
        if (t.joinable()) t.join();
    delete m_impl;
    m_impl = nullptr;
}

bool     JobSystem::Initialized() const { return m_impl != nullptr; }
unsigned JobSystem::WorkerCount() const { return m_impl ? (unsigned)m_impl->workers.size() : 0; }

void JobSystem::Submit(std::function<void()> fn) {
    if (!m_impl) { fn(); return; }                  // fallback: run inline
    m_impl->push({ std::move(fn), nullptr });
}

void JobSystem::Dispatch(WaitGroup& wg, std::function<void()> fn) {
    wg.remaining.fetch_add(1, std::memory_order_relaxed);
    if (!m_impl) {                                  // fallback: run inline
        fn();
        wg.remaining.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }
    m_impl->push({ std::move(fn), &wg });
}

void JobSystem::Wait(WaitGroup& wg) {
    while (wg.remaining.load(std::memory_order_acquire) > 0) {
        Job j;
        if (m_impl && m_impl->tryPop(j)) Impl::run(j);   // help instead of idling
        else                             std::this_thread::yield();
    }
}

void JobSystem::ParallelFor(uint32_t count, const std::function<void(uint32_t)>& fn,
                            uint32_t batchSize) {
    if (count == 0) return;
    if (!m_impl) {                                  // fallback: run inline
        for (uint32_t i = 0; i < count; ++i) fn(i);
        return;
    }
    if (batchSize == 0) {
        unsigned chunks = (unsigned)m_impl->workers.size() * 4u;
        if (chunks == 0) chunks = 1;
        batchSize = (count + chunks - 1) / chunks;
        if (batchSize == 0) batchSize = 1;
    }

    WaitGroup wg;
    for (uint32_t start = 0; start < count; start += batchSize) {
        uint32_t end = std::min(start + batchSize, count);
        Dispatch(wg, [&fn, start, end] {
            for (uint32_t i = start; i < end; ++i) fn(i);
        });
    }
    Wait(wg);   // fn outlives every job because Wait blocks until all complete
}

void JobSystem::EnqueueMainThread(std::function<void()> fn) {
    if (!m_impl) { fn(); return; }
    std::lock_guard<std::mutex> lk(m_impl->mainMtx);
    m_impl->mainTasks.push_back(std::move(fn));
}

void JobSystem::RunMainThreadTasks() {
    if (!m_impl) return;
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard<std::mutex> lk(m_impl->mainMtx);
        tasks.swap(m_impl->mainTasks);
    }
    for (auto& t : tasks) t();
}

} // namespace jobs
