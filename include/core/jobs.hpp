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
#include <rpmalloc.h>
#include <cpu_relax.hpp>
#include <SDL3/SDL.h>
#include <cassert>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>
#include <dbg/metrics.hpp>

struct alignas(8) job {
	void (*exec)(void*){nullptr};
	alignas(8) char payload[48]{}; // 8 for the ptr and 48 payload??

	job() = default;

	template <typename F> static job make(F&& f) {
		using df = std::decay_t<F>;
		// CRUICAL!!
		static_assert(sizeof(df) <= 48, "lambda capture exceeds 48 bytes. NO HEAPS ALLOWED");
		// static_assert(std::is_trivially_destructible_v<std::remove_reference_t<F>>, "must be trivially destructible.");
		static_assert(alignof(df) <= 8, "lambda alignment exceeds 8 bytes");
		static_assert(std::is_trivially_copyable_v<df>, "lambda captures must be trivially copyable (no std::string, no smart pointers)");
		static_assert(std::is_nothrow_invocable_v<df&>, "jobs must not throw: mark the lambda noexcept");

		job j;
		j.exec = [](void* ptr) { (*static_cast<df*>(ptr))(); };

		new (j.payload) df(std::forward<F>(f));

		return j;
	}


	inline void rdispose() noexcept {
		if (exec) {
			exec(payload);

			exec = nullptr;
		}
	}
};

static_assert(sizeof(job) == 56, "job must stay 56 bytes so seq + job is exactly one 64-byte cache line");



template <typename T, size_t Capacity> struct lfmpmc {
	struct alignas(64) cell {
		std::atomic<size_t> seq;
		T data;
	};

	static_assert(!std::is_same_v<T, job> || sizeof(cell) == 64, "seq + job must pack into exactly one 64-byte cache line");

	static_assert((Capacity != 0) && ((Capacity & (~Capacity + 1)) == Capacity), "Capacity must be a power of 2");
	static constexpr size_t bmask = Capacity - 1;

	std::array<cell, Capacity> buffer;

	alignas(std::hardware_destructive_interference_size) std::atomic<size_t> eqpos{0};
	alignas(std::hardware_destructive_interference_size) std::atomic<size_t> dqpos{0};

	lfmpmc() {
		for (size_t i = 0; i < Capacity; ++i) {
			buffer[i].seq.store(i, std::memory_order_relaxed);
		}
	}

	
	[[gnu::always_inline]] inline bool push(T&& data) noexcept {
		cell* ccell = nullptr;
		size_t pos = eqpos.load(std::memory_order_relaxed);

		for (;;) {
			ccell = &buffer[pos & bmask];
			size_t seq = ccell->seq.load(std::memory_order_acquire);
			std::ptrdiff_t dif = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos);

			if (dif == 0) {
				if (eqpos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
					break;
			} else if (dif < 0) {
				return false;
			} else {
				pos = eqpos.load(std::memory_order_relaxed);
			}
		}

		ccell->data = std::move(data);
		ccell->seq.store(pos + 1, std::memory_order_release);
		return true;
	}

	[[gnu::always_inline]] inline bool push(const T& data) noexcept {
		T copy(data);
		return push(std::move(copy));
	}

	bool pop(T& data) noexcept {
		cell* ccell = nullptr;
		size_t pos = dqpos.load(std::memory_order_relaxed);

		for (;;) {
			ccell = &buffer[pos & bmask];
			size_t seq = ccell->seq.load(std::memory_order_acquire);
			std::ptrdiff_t dif = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos + 1);

			if (dif == 0) {
				if (dqpos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
					break;
			} else if (dif < 0) {
				if (pos == eqpos.load(std::memory_order_relaxed)) {
					return false;
				}

				rview::core::cpu_relax();
			} else {
				pos = dqpos.load(std::memory_order_relaxed);
			}
		}

		data = std::move(ccell->data);
		ccell->seq.store(pos + bmask + 1, std::memory_order_release);
		return true;
	}
};

// instead of namespace::cleanup
// will use more memory than regular cleanup
// about 64 bytes per resource instead of 16
// idk if ill use it but it's here
enum struct tdphase : int { logic = 0, graph = 1, pline = 2, buffer = 4, vma = 8, device = 16, instance = 32, window = 64 };
struct teardown {
	teardown(const teardown&) = delete;
	teardown& operator=(const teardown&) = delete;
	teardown() = default;

	struct node {
		node* next;
		int phase;
		std::move_only_function<void()> func;
	};

	
	
	static node* closed() noexcept {
		return reinterpret_cast<node*>(static_cast<uintptr_t>(1));
	}

	
	[[gnu::always_inline]] inline bool enqueue(tdphase phase, std::move_only_function<void()>&& func) {
		node* n = new node{nullptr, static_cast<int>(phase), std::move(func)};
		node* head = pqueue.load(std::memory_order_acquire);

		do {
			if (head == closed()) {
				func = std::move(n->func);
				delete n;
				return false;
			}

			n->next = head;
		} while (!pqueue.compare_exchange_weak(head, n, std::memory_order_acq_rel, std::memory_order_acquire));

		return true;
	}

	[[gnu::always_inline]] inline void fdie() {
		node* head = pqueue.exchange(closed(), std::memory_order_acq_rel);

		if (head == closed())
			return;

		dead.store(true, std::memory_order_release);

		
		
		std::vector<node*> nodes;

		for (node* n = head; n != nullptr; n = n->next) {
			nodes.push_back(n);
		}

		std::stable_sort(nodes.begin(), nodes.end(), [](const node* a, const node* b) { return a->phase < b->phase; });

		
		for (node* n : nodes) {
			n->func();
		}

		for (node* n : nodes) {
			delete n;
		}
	}

	~teardown() {
		node* head = pqueue.exchange(closed(), std::memory_order_acq_rel);

		while (head != nullptr && head != closed()) {
			node* next = head->next;
			delete head;
			head = next;
		}
	}

	std::atomic<bool> dead{false};
	std::atomic<node*> pqueue{nullptr};
};

struct sjguard {
	std::atomic<int>& counter;

	sjguard(std::atomic<int>& c) : counter(c) {}
	~sjguard() {
		counter.fetch_sub(1, std::memory_order_release);
	}
	sjguard(const sjguard&) = delete;
	sjguard& operator=(const sjguard&) = delete;
};
struct jsarena {
	std::vector<char> buffer;
	std::atomic<size_t> offset{0};

	[[gnu::always_inline]] inline jsarena(size_t size) {
		buffer.resize(size);
	}
	[[gnu::always_inline]] inline const char* pstring(std::string_view str) noexcept {
		size_t start = offset.fetch_add(str.size() + 1, std::memory_order_relaxed);

		// add user friendly ":) file path is too long" or some &$#@
		if (start > buffer.size() || str.size() + 1 > buffer.size() - start) [[unlikely]] {
			SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Arena out of memory!");
			std::abort();
		}

		char* dest = &buffer[start];
		memcpy(dest, str.data(), str.size());
		dest[str.size()] = '\0';

		return dest;
	}

	[[gnu::always_inline]] inline void reset() {
		offset.store(0, std::memory_order_relaxed);
	}
};


struct tlarena {
	struct blk {
		std::unique_ptr<uint8_t[]> data;
		size_t capacity;
		size_t offset = 0;

		explicit blk(size_t cap) : data{std::make_unique_for_overwrite<uint8_t[]>(cap)}, capacity{cap} {}
	};

	std::vector<blk> blocks;
	size_t dbsize = 0;
	size_t cbidx = 0; 

	tlarena() = default;

	void init(size_t ibsize) {
		dbsize = ibsize;
		blocks.emplace_back(ibsize);
	}

	
	void prewarm() noexcept {
		for (auto& block : blocks) {
			memset(block.data.get(), 0, block.capacity);
		}
	}

	[[nodiscard]] void* allocate(size_t size, size_t align = 64) {
		assert(std::has_single_bit(align));

		if (blocks.empty()) [[unlikely]] {
			init(dbsize > 0 ? dbsize : 8 * 1024 * 1024);
		}

		blk* cblk = &blocks[cbidx];

		size_t caddr = reinterpret_cast<size_t>(cblk->data.get() + cblk->offset);
		size_t padding = (0 - caddr) & (align - 1);

		while (cblk->offset + padding + size > cblk->capacity) [[unlikely]] {
			
			if (cbidx + 1 == blocks.size()) {
				size_t ncap = std::max(dbsize, size + align);
				blocks.emplace_back(ncap);
			}

			cblk = &blocks[++cbidx];
			caddr = reinterpret_cast<size_t>(cblk->data.get() + cblk->offset);
			padding = (0 - caddr) & (align - 1);
		}

		cblk->offset += padding;
		void* ptr = cblk->data.get() + cblk->offset;
		cblk->offset += size;

		return ptr;
	}

	
	void reset() noexcept {
		for (auto& block : blocks) {
			block.offset = 0;
		}

		cbidx = 0;
	}

	
	void trim() noexcept {
		reset();

		if (blocks.size() > 1) {
			blocks.erase(blocks.begin() + 1, blocks.end());
		}
	}
};

inline constexpr size_t MWTHREADS = 64;



[[gnu::always_inline]] inline bool cidle(std::atomic<int>& nidle) noexcept {
	int n = nidle.load(std::memory_order_seq_cst);

	while (n > 0) {
		if (nidle.compare_exchange_weak(n, n - 1, std::memory_order_seq_cst))
			return true;
	}

	return false;
}

// high: short work a frame is waiting for. low: anything that can run long (file io, parsing, uploads, baking).
enum struct jprio { high, low };
struct threadpool {
	inline static constexpr uint32_t HPQCAP = 1024;
	inline static constexpr uint32_t LPQCAP = 4096;
	
	
	inline static constexpr uint32_t WSPINS = 0;
	
	
	inline static constexpr uint32_t HWORKERS = 1;

	threadpool() = default;

	void init() {
		if (isrun() || !workers.empty() || stopped.load(std::memory_order_acquire)) {
			SDL_Log("threadpool already initialized");
			return;
		}

		int32_t hc = static_cast<int32_t>(std::thread::hardware_concurrency());
		uint32_t nthreads = static_cast<uint32_t>(std::max(2, hc - 2));
		uint32_t nlow = nthreads > HWORKERS ? nthreads - HWORKERS : 1;

		running.store(true, std::memory_order_release);

		for (uint32_t i = 0; i < nthreads; ++i) {
			try {
				workers.emplace_back(&threadpool::wloop, this, i < nlow);
			} catch (const std::system_error& e) {
				SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "failed to spawn worker %u:\n %s", i, e.what());
			}
		}
	}

	bool isrun() const {
		return running.load(std::memory_order_acquire);
	}

	void sjoin() {
		if (stopped.exchange(true, std::memory_order_seq_cst))
			return;

		running.store(false, std::memory_order_seq_cst);
		std::atomic_thread_fence(std::memory_order_seq_cst);

		
		while (cidle(nidle)) {
			javail.release();
		}

		while (cidle(hnidle)) {
			hjavail.release();
		}

		for (auto& w : workers) {
			if (w.joinable()) {
				w.join();
			}
		}

		
		while (hexec(true)) {}
	}

	~threadpool() {
		sjoin();
	}

	template <typename F> void enqueue(F&& f, jprio priority = jprio::high) {
		job njob = job::make(std::forward<F>(f));

		
		if (stopped.load(std::memory_order_acquire)) [[unlikely]] {
			njob.rdispose();
			return;
		}

		gm.jenq(priority == jprio::low);

		bool full = false;

		if (priority == jprio::high) {
			while (!hpqueue.push(njob)) {
				full = true;

				if (!hexec(hlow()))
					rview::core::cpu_relax();
			}
		} else {
			while (!lpqueue.push(njob)) {
				full = true;

				if (!hexec(hlow()))
					rview::core::cpu_relax();
			}
		}

		if (full) [[unlikely]]
			nfull.fetch_add(1, std::memory_order_relaxed);

		
		
		std::atomic_thread_fence(std::memory_order_seq_cst);

		
		
		if (priority == jprio::high && cidle(hnidle)) {
			hjavail.release();
			gm.jrel(true);
		} else if (cidle(nidle)) {
			gm.jrel(false);
			javail.release();
		}

		
		if (stopped.load(std::memory_order_seq_cst)) [[unlikely]] {
			while (hexec(true)) {}
		}
	}

	
	
	bool hlow() const noexcept {
		return tlow || !running.load(std::memory_order_acquire);
	}

	
	bool hexec(bool lowok = true) noexcept {
		job cjob;

		// X_X
		// no more javail.try_acquire();
		if (hpqueue.pop(cjob)) {
			bool plow = std::exchange(tlow, false);
			const uint64_t mt = gm.jbegin();
			cjob.rdispose();
			gm.jend(mt, false);
			tlow = plow;
			return true;
		}

		if (lowok && lpqueue.pop(cjob)) {
			bool plow = std::exchange(tlow, true);
			const uint64_t mt = gm.jbegin();
			cjob.rdispose();
			gm.jend(mt, true);
			tlow = plow;
			return true;
		}

		return false;
	}

	
	void wjobs(std::atomic<int>& pcount) noexcept {
		const uint64_t mt = gm.wjbegin();
		while (pcount.load(std::memory_order_acquire) > 0) {
			if (!hexec(hlow()))
				rview::core::cpu_relax();
		}

		gm.wjend(mt);
	}

	
	void msample() const noexcept {
		auto depth = [](const auto& q) noexcept {
			const std::ptrdiff_t d = static_cast<std::ptrdiff_t>(q.eqpos.load(std::memory_order_relaxed) - q.dqpos.load(std::memory_order_relaxed));
			return static_cast<uint32_t>(d > 0 ? d : 0);
		};

		gm.jsample(depth(hpqueue), depth(lpqueue), static_cast<uint32_t>(nidle.load(std::memory_order_relaxed)),
				   static_cast<uint32_t>(hnidle.load(std::memory_order_relaxed)), nfull.load(std::memory_order_relaxed));
	}

	
	template <typename F> void pfor(uint32_t total, uint32_t bsize, F&& fn) {
		
		static_assert(std::is_nothrow_invocable_v<F&, uint32_t, uint32_t>, "pfor body must not throw: mark it noexcept");

		if (total == 0)
			return;

		if (bsize == 0) [[unlikely]]
			bsize = total;

		
		uint32_t nbatches = static_cast<uint32_t>((static_cast<uint64_t>(total) + bsize - 1) / bsize);

		if (nbatches == 1) {
			fn(0u, total);
			return;
		}

		assert(nbatches - 1 <= static_cast<uint32_t>(std::numeric_limits<int>::max()));
		std::atomic<int> pcount{static_cast<int>(nbatches - 1)};

		for (uint32_t i = 0; i < nbatches - 1; ++i) {
			uint32_t bstart = i * bsize;
			uint32_t bend = bstart + bsize;

			
			enqueue([pfn = std::addressof(fn), pc = &pcount, bstart, bend]() noexcept {
				sjguard guard(*pc);
				(*pfn)(bstart, bend);
			});
		}

		fn((nbatches - 1) * bsize, total);
		wjobs(pcount);
	}

	lfmpmc<job, HPQCAP> hpqueue;
	lfmpmc<job, LPQCAP> lpqueue;
	// $&#@ it we ball x2
	std::counting_semaphore<std::numeric_limits<std::ptrdiff_t>::max()> javail{0};
	std::counting_semaphore<std::numeric_limits<std::ptrdiff_t>::max()> hjavail{0};
	std::atomic<int> nidle{0};  
	std::atomic<int> hnidle{0}; 
	std::atomic<bool> running{false};
	std::atomic<bool> stopped{false};
	std::atomic<uint64_t> nfull{0}; 

	std::vector<std::thread> workers;
	inline static thread_local bool tlow = false; 

	void wloop(bool lowok) {
		rdebug::set_thread_name("worker");

		rpmalloc_thread_initialize();
		gm.wkind(lowok ? 1 : 2);

		std::atomic<int>& cnidle = lowok ? nidle : hnidle;
		auto& cjavail = lowok ? javail : hjavail;

		for (;;) {
			{
				ZoneScopedN("jobexec");

				while (hexec(lowok)) {}
			}

			if (!running.load(std::memory_order_acquire))
				break;

			// stay hot for a moment
			bool found = false;

			for (uint32_t i = 0; i < WSPINS && !found; ++i) {
				rview::core::cpu_relax();
				found = hexec(lowok);
			}

			if (found)
				continue;

			
			
			cnidle.fetch_add(1, std::memory_order_seq_cst);
			std::atomic_thread_fence(std::memory_order_seq_cst);

			if (hexec(lowok) || !running.load(std::memory_order_seq_cst)) {
				
				
				if (cidle(cnidle))
					continue;
			}

			rpmalloc_thread_collect();
			const uint64_t mt = mnow();
			cjavail.acquire();
			gm.jwoke(mt, !lowok, running.load(std::memory_order_relaxed));
		}

		rpmalloc_thread_finalize(1);
	}
};

struct gcworker {
	gcworker() = default;

	void init() {

		if (worker.joinable() || running.load(std::memory_order_acquire) || stopped.load(std::memory_order_acquire)) {
			SDL_Log("reaper init() called but was already running");
			return;
		}

		running.store(true, std::memory_order_release);
		// abi mismatch XP
		try {
			worker = std::thread(&gcworker::loop, this);
		} catch (const std::system_error& e) {
			SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, " OS rejected reaper std::thread creation:\n %s", e.what());
			running.store(false, std::memory_order_release);
		}
	}

	void sjoin() {
		if (stopped.exchange(true, std::memory_order_seq_cst))
			return;

		running.store(false, std::memory_order_seq_cst);
		std::atomic_thread_fence(std::memory_order_seq_cst);

		if (cidle(nidle))
			iavail.release();

		if (worker.joinable())
			worker.join();

		// collecting stragglers
		while (hexec()) {}
	}

	~gcworker() {
		sjoin();
	}

	template <typename F> void enqueue(F&& f) {
		job njob = job::make(std::forward<F>(f));

		
		if (stopped.load(std::memory_order_acquire)) [[unlikely]] {
			njob.rdispose();
			return;
		}

		while (!queue.push(njob)) {
			
			
			if (tself == this || !running.load(std::memory_order_acquire)) {
				hexec();
			} else {
				rview::core::cpu_relax();
			}
		}

		
		std::atomic_thread_fence(std::memory_order_seq_cst);

		if (cidle(nidle))
			iavail.release();

		if (stopped.load(std::memory_order_seq_cst)) [[unlikely]] {
			while (hexec()) {}
		}
	}

	bool hexec() noexcept {
		job cjob;

		if (queue.pop(cjob)) {
			cjob.rdispose();
			return true;
		}

		return false;
	}

	lfmpmc<job, 1024> queue;
	std::counting_semaphore<std::numeric_limits<std::ptrdiff_t>::max()> iavail{0};
	std::atomic<int> nidle{0};
	std::atomic<bool> running{false};
	std::atomic<bool> stopped{false};
	std::thread worker;
	inline static thread_local const gcworker* tself = nullptr;

	void loop() {
		rdebug::set_thread_name("reaper");
		rpmalloc_thread_initialize();
		tself = this;

		for (;;) {
			{
				ZoneScopedN("reaper_gc");

				while (hexec()) {}
			}

			if (!running.load(std::memory_order_acquire))
				break;

			
			nidle.fetch_add(1, std::memory_order_seq_cst);
			std::atomic_thread_fence(std::memory_order_seq_cst);

			if (hexec() || !running.load(std::memory_order_seq_cst)) {
				if (cidle(nidle))
					continue;
			}

			rpmalloc_thread_collect();
			iavail.acquire();
		}

		rpmalloc_thread_finalize(1);
	}
};

// destruction order (last declared dies first)

alignas(64) inline std::array<std::atomic<bool>, 8192> gctokens{}; // magic

inline std::atomic<int> gaiojobs{0};
inline teardown gexitq;
inline jsarena gjstrings(1024 * 1024);

inline std::array<tlarena, MWTHREADS> gtlarenas;
inline std::atomic<uint32_t> gacount{0};
inline thread_local uint32_t taidx = 0xFFFFFFFF;

// temp memory for any job that requires [[invoke]]
inline tlarena& ctlarena() {
	if (taidx == 0xFFFFFFFF) [[unlikely]] {
		uint32_t idx = gacount.fetch_add(1, std::memory_order_relaxed);

		if (idx >= MWTHREADS) [[unlikely]] {
			SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "ctlarena: out of thread arena slots (%u)", static_cast<unsigned>(MWTHREADS));
			std::abort();
		}

		taidx = idx;
		gtlarenas[taidx].init(8 * 1024 * 1024);
	}

	return gtlarenas[taidx];
}

// idk which is better, fan out or this but here it is exclusively for GC
inline gcworker greaper{};

inline threadpool gjobs{};
