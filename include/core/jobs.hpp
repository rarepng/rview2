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
#include <cstddef>
#include <functional>
#include <map>
#include <dbg/trace.hpp>
#include <condition_variable>

struct alignas(64) job {
	void (*execute)(void*) {
		nullptr
	};
	void (*destroy)(void*) {
		nullptr
	};
	alignas(16) char payload[48] {}; // 8+8 for the ptrs and 48 payload

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
		stop_and_join_all();
	}


	template<typename F>
	void enqueue(F&& f) {
		uint32_t next_tail;
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			next_tail = (tail + 1) & QUEUE_MASK;

			queue_not_full_cv.wait(lock, [this, next_tail]() {
				return next_tail != head;
			});

			ring_buffer[tail] = job::make(std::forward<F>(f));
			tail = next_tail;
		}

		jobs_available.release();
	}

	std::condition_variable queue_not_full_cv;

private:
	alignas(64) std::array<job, QUEUE_SIZE> ring_buffer{};
	uint32_t head{0};
	uint32_t tail{0};

	std::mutex queue_mutex;
	std::counting_semaphore<QUEUE_SIZE> jobs_available{0};
	std::atomic<bool> running{false};

	std::vector<std::thread> workers;

	void worker_loop() {
		rdebug::set_thread_name("worker");

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
			queue_not_full_cv.notify_one();
			ZoneScopedN("jobexec");
			current_job.run_and_dispose();

		}
	}
};

inline threadpool g_jobs{};


template<typename T, size_t Capacity>
class LockFreeMPMC {
private:
	struct Cell {
		std::atomic<size_t> sequence;
		T data;
	};

	static_assert((Capacity != 0) && ((Capacity & (~Capacity + 1)) == Capacity),
	              "Capacity must be a power of 2");
	static constexpr size_t buffer_mask = Capacity - 1;

	std::array<Cell, Capacity> buffer;

	alignas(std::hardware_destructive_interference_size) std::atomic<size_t> enqueue_pos{0};
	alignas(std::hardware_destructive_interference_size) std::atomic<size_t> dequeue_pos{0};

public:
	LockFreeMPMC() {
		for (size_t i = 0; i < Capacity; ++i) {
			buffer[i].sequence.store(i, std::memory_order_relaxed);
		}
	}

	bool push(T data) {
		Cell* cell = nullptr;
		size_t pos = enqueue_pos.load(std::memory_order_relaxed);

		for (;;) {
			cell = &buffer[pos & buffer_mask];
			size_t seq = cell->sequence.load(std::memory_order_acquire);
			std::ptrdiff_t dif = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos);

			if (dif == 0) {
				if (enqueue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) break;
			} else if (dif < 0) {
				return false;
			} else {
				pos = enqueue_pos.load(std::memory_order_relaxed);
			}
		}

		cell->data = std::move(data);
		cell->sequence.store(pos + 1, std::memory_order_release);
		return true;
	}

	bool pop(T& data) {
		Cell* cell = nullptr;
		size_t pos = dequeue_pos.load(std::memory_order_relaxed);

		for (;;) {
			cell = &buffer[pos & buffer_mask];
			size_t seq = cell->sequence.load(std::memory_order_acquire);
			std::ptrdiff_t dif = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos + 1);

			if (dif == 0) {
				if (dequeue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) break;
			} else if (dif < 0) {
				return false;
			} else {
				pos = dequeue_pos.load(std::memory_order_relaxed);
			}
		}

		data = std::move(cell->data);
		cell->sequence.store(pos + buffer_mask + 1, std::memory_order_release);
		return true;
	}
};

// instead of namespace::cleanup
// will use more memory than regular cleanup
// about 64 bytes per resource instead of 16
// idk if ill use it but it's here
enum class TeardownPhase : int {
	logic = 0,
	graph = 1,
	pline = 2,
	buffer = 4,
	vma = 8,
	device = 16,
	instance = 32,
	window = 64
};
class teardown {
public:
	teardown(const teardown&) = delete;
	teardown& operator=(const teardown&) = delete;
	teardown() = default;

	void enqueue(TeardownPhase phase, std::move_only_function<void()>&& func) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_queue[static_cast<int>(phase)].push_back(std::move(func));
	}

	void flush_and_die() {
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_is_dead) return;

		m_is_dead = true;

		for (auto& [phase, callbacks] : m_queue) {
			for (auto it = callbacks.rbegin(); it != callbacks.rend(); ++it) {
				(*it)();
			}
		}

		m_queue.clear();
	}

private:
	std::mutex m_mutex;
	bool m_is_dead = false;
	std::map<int, std::vector<std::move_only_function<void()>>> m_queue;
};

inline teardown g_exitQ;
struct ScopedJobGuard {
	std::atomic<int>& counter;

	ScopedJobGuard(std::atomic<int>& c) : counter(c) {}
	~ScopedJobGuard() {
		counter.fetch_sub(1, std::memory_order_release);
	}
	ScopedJobGuard(const ScopedJobGuard&) = delete;
	ScopedJobGuard& operator=(const ScopedJobGuard&) = delete;
};
class JobStringArena {
	std::vector<char> buffer;
	std::atomic<size_t> offset{0};

public:
	JobStringArena(size_t size) {
		buffer.resize(size);
	}
	const char* push_string(std::string_view str) {
		size_t start = offset.fetch_add(str.size() + 1, std::memory_order_relaxed);

		// add user friendly ":) file path is too long" or some &$#@
		assert(start + str.size() + 1 <= buffer.size() && "Arena out of memory!");

		char* dest = &buffer[start];
		memcpy(dest, str.data(), str.size());
		dest[str.size()] = '\0';

		return dest;
	}

	void reset() {
		offset.store(0, std::memory_order_relaxed);
	}
};

inline JobStringArena g_job_strings(1024 * 1024);
inline std::atomic<int> active_io_jobs{0};