#pragma once
#include <functional>
#include <atomic>
#include <cstdint>

// CPU job system: a worker thread pool with cooperative waiting. Rendering stays
// on the main thread (the GL context is thread-affine); this parallelizes CPU
// work (asset import, animation, culling, ECS systems) and marshals results that
// must touch GL/the registry back to the main thread.
namespace jobs {

// Cooperative wait counter. Dispatch() increments it, a worker decrements it when
// the job finishes, Wait() blocks until it reaches zero. Hold one on the stack
// across a Dispatch/Wait pair; do not copy or move it.
struct WaitGroup {
    std::atomic<int> remaining{ 0 };
    WaitGroup() = default;
    WaitGroup(const WaitGroup&) = delete;
    WaitGroup& operator=(const WaitGroup&) = delete;
};

class JobSystem {
public:
    static JobSystem& Get();

    // workerCount == 0 -> max(1, hardware_concurrency() - 1). Idempotent.
    void Initialize(unsigned workerCount = 0);
    void Shutdown();
    bool Initialized() const;
    unsigned WorkerCount() const;

    // Fire-and-forget background task. Runs inline if the system isn't running.
    void Submit(std::function<void()> fn);

    // Tracked task: increments wg now, decrements it after fn() returns.
    void Dispatch(WaitGroup& wg, std::function<void()> fn);

    // Block until wg hits zero, executing pending jobs meanwhile — so the caller
    // never idles and a worker waiting on its own children can't deadlock.
    void Wait(WaitGroup& wg);

    // Process [0, count) across workers; the calling thread participates and the
    // call blocks until every item is done. batchSize == 0 -> ~4 chunks/worker.
    void ParallelFor(uint32_t count, const std::function<void(uint32_t)>& fn,
                     uint32_t batchSize = 0);

    // ── Main-thread marshaling ──────────────────────────────────────────────
    // Thread-safe enqueue; the callback runs when RunMainThreadTasks() is next
    // called (the editor drains it once per frame). Use for results that touch
    // GL or the ECS registry.
    void EnqueueMainThread(std::function<void()> fn);
    void RunMainThreadTasks();

private:
    JobSystem() = default;
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace jobs
