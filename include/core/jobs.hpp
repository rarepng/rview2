#pragma once
#include <thread>
#include <atomic>
#include <array>
#include <vector>
#include <type_traits>
#include <new>
#include <memory>
#include <mutex>
#include <semaphore>
#include <tracy/Tracy.hpp>

struct alignas(64) job {
    void (*execute)(void*){nullptr};
    void (*destroy)(void*){nullptr};
    alignas(16) char payload[48]{}; // 8+8 for the ptrs and 48 payload

    job() = default;

    template<typename F>
    static job make(F&& f) {
        using DecayedF = std::decay_t<F>;
        // CRUICAL!!
        static_assert(sizeof(DecayedF) <= 48, "lambda capture exceeds 56 bytes. NO HEAPS ALLOWED");
        // static_assert(std::is_trivially_destructible_v<std::remove_reference_t<F>>, "must be trivially destructible.");
        static_assert(alignof(DecayedF) <= 16, "lambda alignment exceeds 16 bytes");

        job j;
        j.execute = [](void* ptr) {
            (*static_cast<DecayedF*>(ptr))();
        };
        
        if constexpr (!std::is_trivially_destructible_v<DecayedF>) {
            j.destroy = [](void* ptr) {
                static_cast<DecayedF*>(ptr)->~DecayedF();
            };
        }

        void* target = j.payload;
        size_t space = 48;
        std::align(alignof(DecayedF), sizeof(DecayedF), target, space);
        
        new (target) DecayedF(std::forward<F>(f));
        
        return j;
    }

    
    inline void run_and_dispose() {
        if (execute) {
            execute(payload);
            if (destroy) destroy(payload);
            execute = nullptr;
            destroy = nullptr;
        }
    }
};

class threadpool {
public:
    inline static constexpr uint32_t QUEUE_SIZE = 1024;
    inline static constexpr uint32_t QUEUE_MASK = QUEUE_SIZE - 1;

    threadpool() {
        uint32_t num_threads = std::thread::hardware_concurrency();
        if (num_threads <= 1) num_threads = 2;

        running.store(true, std::memory_order_release);
        for (uint32_t i = 0; i < num_threads - 1; ++i) {
            workers.emplace_back(&threadpool::worker_loop, this);
        }
    }
    void stop_and_join_all() {
        if (running.exchange(false, std::memory_order_acq_rel)) {
            jobs_available.release(workers.size() * 2);
            for (auto& w : workers) {
                if (w.joinable()) {
                    w.join();
                }
            }
        }
    }


    ~threadpool() {
        running.store(false, std::memory_order_release);
        jobs_available.release(workers.size() * 2);
        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }
        stop_and_join_all();
    }
        

    template<typename F>
    void enqueue(F&& f) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        uint32_t next_tail = (tail + 1) & QUEUE_MASK;

        // i was told this is for (backpressure) // recheck and verify
        while (next_tail == head) {
            queue_mutex.unlock();
            std::this_thread::yield();
            queue_mutex.lock();
        }
        ring_buffer[tail] = job::make(std::forward<F>(f));
        tail = next_tail;

        jobs_available.release();
    }

private:
    alignas(64) std::array<job, QUEUE_SIZE> ring_buffer{};
    uint32_t head{0};
    uint32_t tail{0};
    
    std::mutex queue_mutex;
    std::counting_semaphore<QUEUE_SIZE> jobs_available{0};
    std::atomic<bool> running{false};

    std::vector<std::thread> workers;

    void worker_loop() {
        tracy::SetThreadName("worker");
        while (running.load(std::memory_order_acquire)) {
            jobs_available.acquire();
            if (!running.load(std::memory_order_acquire)) return;

            job current_job;

            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (head == tail) continue;
                
                current_job = ring_buffer[head];
                
                ring_buffer[head].execute = nullptr;
                ring_buffer[head].destroy = nullptr;
                
                head = (head + 1) & QUEUE_MASK;
            }
            ZoneScopedN("jobexec");
            current_job.run_and_dispose();

        }
    }
};

inline threadpool g_jobs{};



