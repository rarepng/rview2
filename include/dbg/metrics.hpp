#pragma once


#include <SDL3/SDL.h>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifndef RVIEW_METRICS
#define RVIEW_METRICS 1
#endif

#ifndef RVIEW_CONFIG
#define RVIEW_CONFIG "unknown"
#endif

enum struct mphase : uint8_t { loading, steady, count };
enum struct mstage : uint8_t { fence, setup, acquire, events, intake, commit, deathrow, kills, statechg, quiet, poolprep, dispatch, instances, assetsync, record, graph, ui, lockwait, submit, present, tail, count };
enum struct mmark : uint8_t { mainentry, initdone, firstframe, firstmodel, allboot, quit, joined, cleaned, count };
enum struct mstep : uint8_t { queued, started, bakebegin, bakeend, parsed, committed, pushed, ended, spawned, count };
enum struct mbench : uint8_t { none, boot, quit, count };

// names for the report, in enum order
inline constexpr const char* mphasename[] = { "loading", "steady" };
inline constexpr const char* mstagename[] = { "fence", "setup", "acquire", "events", "intake", "commit", "deathrow", "kills", "statechg", "quiet", "poolprep", "dispatch", "instances", "assetsync", "record", "graph", "ui", "lockwait", "submit", "present", "tail" };
inline constexpr const char* mmarkname[] = { "mainentry", "initdone", "firstframe", "firstmodel", "allboot", "quit", "joined", "cleaned" };
inline constexpr const char* mkindname[] = { "unknown", "general", "highonly", "main" };
inline constexpr const char* mbenchname[] = { "none", "boot", "quit" };

static_assert(sizeof(mphasename) / sizeof(mphasename[0]) == static_cast<uint32_t>(mphase::count));
static_assert(sizeof(mstagename) / sizeof(mstagename[0]) == static_cast<uint32_t>(mstage::count));
static_assert(sizeof(mmarkname) / sizeof(mmarkname[0]) == static_cast<uint32_t>(mmark::count));
static_assert(sizeof(mbenchname) / sizeof(mbenchname[0]) == static_cast<uint32_t>(mbench::count));
 
struct mhist {
	inline static constexpr uint32_t SUB = 32;
	inline static constexpr uint32_t NB = 60 * SUB;

	uint32_t b[NB]{};
	uint64_t n = 0, sum = 0, lo = UINT64_MAX, hi = 0;

	static uint32_t idx(uint64_t v) noexcept {
		if (v < SUB)
			return static_cast<uint32_t>(v);

		const uint32_t e = 63u - static_cast<uint32_t>(std::countl_zero(v));
		return (e - 4u) * SUB + static_cast<uint32_t>((v >> (e - 5u)) & (SUB - 1u));
	}
	static uint64_t edge(uint32_t i) noexcept {
		if (i < SUB)
			return i;

		const uint32_t e = i / SUB + 4u;
		const uint64_t m = SUB + (i % SUB);
		return ((m + 1u) << (e - 5u)) - 1u;
	}

	void add(uint64_t v) noexcept {
		++b[idx(v)];
		++n;
		sum += v;
		lo = v < lo ? v : lo;
		hi = v > hi ? v : hi;
	}

	// p in [0, 1]. returns ticks
	uint64_t pct(double p) const noexcept {
		if (n == 0)
			return 0;

		uint64_t want = static_cast<uint64_t>(std::ceil(p * static_cast<double>(n)));
		want = want < 1 ? 1 : (want > n ? n : want);
		uint64_t seen = 0;

		for (uint32_t i = 0; i < NB; ++i) {
			seen += b[i];

			if (seen >= want) {
				const uint64_t e = edge(i);
				return e > hi ? hi : e;
			}
		}

		return hi;
	}

	// samples above v, counted by whole buckets
	uint64_t above(uint64_t v) const noexcept {
		uint64_t c = 0;

		for (uint32_t i = idx(v) + 1; i < NB; ++i)
			c += b[i];

		return c;
	}

	void clear() noexcept {
		for (uint32_t i = 0; i < NB; ++i)
			b[i] = 0;

		n = 0;
		sum = 0;
		lo = UINT64_MAX;
		hi = 0;
	}

	void merge(const mhist& o) noexcept {
		if (o.n == 0)
			return;

		for (uint32_t i = 0; i < NB; ++i)
			b[i] += o.b[i];

		n += o.n;
		sum += o.sum;
		lo = o.lo < lo ? o.lo : lo;
		hi = o.hi > hi ? o.hi : hi;
	}
};

bool mos_mem(uint64_t& workingset, uint64_t& privatebytes) noexcept;
double mos_startms() noexcept;

#if RVIEW_METRICS

#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>

#ifdef TRACY_ENABLE
inline constexpr bool mtracy = true; // !!
using mzctx = TracyCZoneCtx;

inline constexpr ___tracy_source_location_data mzloc[] = {
	{ "fence", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "setup", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "acquire", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "events", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "intake", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "commit", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "deathrow", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "kills", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "statechg", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "quiet", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "poolprep", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "dispatch", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "instances", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "assetsync", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "record", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "graph", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "ui", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "lockwait", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "submit", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "present", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 },
	{ "tail", "draw", __FILE__, static_cast<uint32_t>(__LINE__), 0 }
};

static_assert(sizeof(mzloc) / sizeof(mzloc[0]) == static_cast<uint32_t>(mstage::count));

inline mzctx mzbegin(mstage s) noexcept { return ___tracy_emit_zone_begin(&mzloc[static_cast<uint32_t>(s)], 1); }
inline void mzend(mzctx c) noexcept { ___tracy_emit_zone_end(c); }
inline void mzmsg(const char* s, std::size_t n) noexcept { TracyMessage(s, n); }
#else
inline constexpr bool mtracy = false;
struct mzctx {};
inline mzctx mzbegin(mstage) noexcept { return {}; }
inline void mzend(mzctx) noexcept {}
inline void mzmsg(const char*, std::size_t) noexcept {}
#endif

#ifdef RVIEW_DEMO
inline constexpr int mdemo = RVIEW_DEMO ? 1 : 0;
#else
inline constexpr int mdemo = 0;
#endif

inline uint64_t mnow() noexcept {
	return SDL_GetPerformanceCounter();
}

inline double mms(uint64_t t) noexcept;

struct alignas(64) mwslot {
	std::atomic<uint64_t> kind{0};
	std::atomic<uint64_t> enq[2]{};
	std::atomic<uint64_t> done[2]{};
	std::atomic<uint64_t> busy{0};
	std::atomic<uint64_t> helped{0};
	std::atomic<uint64_t> helpedjobs{0};
	std::atomic<uint64_t> waited{0};
	std::atomic<uint64_t> waits{0};
	std::atomic<uint64_t> parks{0};
	std::atomic<uint64_t> parked{0};
	std::atomic<uint64_t> maxdepth{0};
	uint32_t depth = 0;
	mhist run[2]{}; // 0 high, 1 low
	mhist wake{};
	mhist animwait[2]{}; // 0 loading, 1 steady
	mhist animrun[2]{}; // //
};

struct metrics {
	inline static constexpr uint32_t NPH = static_cast<uint32_t>(mphase::count);
	inline static constexpr uint32_t NST = static_cast<uint32_t>(mstage::count);
	inline static constexpr uint32_t NMARK = static_cast<uint32_t>(mmark::count);
	inline static constexpr uint32_t NSTEP = static_cast<uint32_t>(mstep::count);
	inline static constexpr uint32_t NLOADS = 256;
	inline static constexpr uint32_t NOLOAD = 0xFFFFFFFFu;
	inline static constexpr uint32_t NSLOTS = 64;
	inline static constexpr uint32_t NWORST = 32;
	inline static constexpr uint32_t NLOG = 1u << 16;

	struct pstat {
		mhist work;
		mhist interval;
		mhist stage[NST];
		mhist gpu;
		uint64_t frames = 0;
		uint64_t quietframes = 0;
	};

	
	struct wrec {
		uint64_t frame, work, interval;
		uint64_t st[NST];
		mphase phase;
		bool quiet;
	};

	struct frec {
		uint32_t work, interval;
		mphase phase;
	};

	struct lrec {
		char name[48];
		std::atomic<uint64_t> step[NSTEP];
		std::atomic<uint32_t> depth;
		std::atomic<uint32_t> clips;
		std::atomic<uint32_t> joints;
	};

	
	uint64_t tbegin = 0;  
	uint64_t tstage = 0;  
	uint64_t tend = 0;    
	uint64_t work = 0;    
	uint64_t st[NST]{};   
	uint8_t cur = 0;      
	bool open = false;    
	bool live = false;    
	bool closed = false;  
	bool fq = false;      
	mzctx zctx{};
	bool zopen = false;

	
	
	std::atomic<uint64_t> uiticks{0};

	uint64_t freq = 0;    
	uint64_t nframes = 0; 
	uint32_t nworst = 0;  
	uint32_t wlo = 0;     

	pstat ps[NPH]{};
	wrec worst[NWORST]{};
	frec flog[NLOG]{};

	std::atomic<uint64_t> marks[NMARK]{};
	std::atomic<uint32_t> nloads{0};
	lrec loads[NLOADS]{};

	std::atomic<uint8_t> curphase{0};
	std::atomic<uint64_t> trel[2]{}; 
	std::atomic<uint32_t> nslots{0};
	mwslot slots[NSLOTS + 1]{};

	mhist qdepth[2]{}; // 0 high, 1 low
	uint64_t idlesum = 0;
	uint64_t hidlesum = 0;
	uint64_t samples = 0;
	uint64_t qfull = 0;

	mbench bmode = mbench::none;
	double bsecs = 0.0;
	bool bnc = false;

	double procms = -1.0;
	uint64_t tmem = 0;
	uint64_t wlast = 0, wpeak = 0;
	uint64_t plast = 0, ppeak = 0;
	uint64_t vused = 0, vpeak = 0, vbud = 0;

	inline static thread_local uint32_t curload = NOLOAD;
	inline static thread_local uint32_t curdepth = 0;
	inline static thread_local mwslot* myslot = nullptr;
	
	void fbegin() noexcept {
		for (uint32_t i = 0; i < NST; ++i)
			st[i] = 0;

		open = false;

		if constexpr (mtracy)
			zopen = false;

		live = true;
		tbegin = mnow();
	}

	
	void stage(mstage s) noexcept {
		const uint64_t t = mnow();

		if (!live || s >= mstage::count)
			return;

		if (open)
			st[cur] += t - tstage;

		if constexpr (mtracy) {
			if (zopen)
				mzend(zctx);

			zctx = mzbegin(s);
			zopen = true;
		}

		cur = static_cast<uint8_t>(s);
		tstage = t;
		open = true;
	}

	
	void quiet(bool q) noexcept {
		fq = q;
	}

	
	
	void uipass(uint64_t d) noexcept {
		uiticks.fetch_add(d, std::memory_order_relaxed);
	}

	
	void fclose() noexcept {
		const uint64_t t = mnow();

		if (!live)
			return;

		if (open)
			st[cur] += t - tstage;

		if constexpr (mtracy) {
			if (zopen) {
				mzend(zctx);
				zopen = false;
			}
		}

		open = false;
		live = false;
		closed = true;
		work = t - tbegin;
	}

	
	void fend(bool loading) noexcept {
		if (!closed)
			return;

		closed = false;
		st[static_cast<uint32_t>(mstage::ui)] += uiticks.exchange(0, std::memory_order_relaxed);

		const uint64_t t = mnow();
		const bool first = nframes == 0;
		const uint64_t iv = first ? 0 : t - tend; 
		tend = t;

		if (first)
			mark(mmark::firstframe);

		if (!loading && nloads.load(std::memory_order_relaxed) > 0)
			mark(mmark::allboot);

		const mphase ph = loading ? mphase::loading : mphase::steady;
		curphase.store(static_cast<uint8_t>(ph), std::memory_order_relaxed);
		pstat& p = ps[static_cast<uint32_t>(ph)];

		p.work.add(work);

		if (!first)
			p.interval.add(iv);

		for (uint32_t i = 0; i < NST; ++i)
			p.stage[i].add(st[i]);

		++p.frames;
		p.quietframes += fq ? 1u : 0u;

		
		if (nworst < NWORST) {
			wput(nworst++, ph, iv);

			if (nworst == NWORST)
				wfind();
		} else if (work > worst[wlo].work) {
			wput(wlo, ph, iv);
			wfind();
		}

		frec& r = flog[nframes & (NLOG - 1u)];
		r.work = clamp32(work);
		r.interval = clamp32(iv);
		r.phase = ph;

		TracyPlot("frame work ms", mms(work));
		TracyPlot("frame interval ms", mms(iv));

		if constexpr (mtracy) {
			const double ivms = mms(iv);

			if (ivms > 16.67) {
				uint32_t top = 0;

				for (uint32_t i = 1; i < NST; ++i)
					if (st[i] > st[top])
						top = i;

				char buf[128];
				const int n = std::snprintf(buf, sizeof(buf), "hitch: interval %.2f ms, work %.2f ms, %s %.2f ms",
					ivms, mms(work), mstagename[top], mms(st[top]));

				if (n > 0) {
					const std::size_t len = static_cast<std::size_t>(n) < sizeof(buf) ? static_cast<std::size_t>(n) : sizeof(buf) - 1u;
					mzmsg(buf, len);
				}
			}
		}

		if (tmem == 0 || (t >= tmem && mms(t - tmem) >= 1000.0)) {
			tmem = t;
			uint64_t ws = 0, priv = 0;

			if (mos_mem(ws, priv)) {
				wlast = ws;
				wpeak = ws > wpeak ? ws : wpeak;
				plast = priv;
				ppeak = priv > ppeak ? priv : ppeak;
			}
		}

		++nframes;
		fq = false;
	}

	
	void gpu(uint64_t ns) noexcept {
		ps[curphase.load(std::memory_order_relaxed) ? 1u : 0u].gpu.add(ns);
	}

	
	void vmem(uint64_t used, uint64_t budget) noexcept {
		vused = used;
		vpeak = used > vpeak ? used : vpeak;
		vbud = budget;
	}

	void mark(mmark m) noexcept {
		if (m >= mmark::count)
			return;

		std::atomic<uint64_t>& slot = marks[static_cast<uint32_t>(m)];

		if (slot.load(std::memory_order_relaxed) == 0) {
			slot.store(mnow(), std::memory_order_relaxed);

			if (m == mmark::mainentry)
				procms = mos_startms();
		}
	}

	
	uint32_t lnew(const char* path) noexcept {
		const uint32_t id = nloads.load(std::memory_order_relaxed);

		if (id >= NLOADS)
			return NOLOAD;

		lrec& l = loads[id];
		const char* base = path ? path : "";

		for (const char* p = base; *p; ++p)
			if (*p == '/' || *p == '\\')
				base = p + 1;

		uint32_t len = 0;

		for (; len < 47u && base[len]; ++len)
			l.name[len] = base[len];

		
		while (len > 0 && (static_cast<unsigned char>(base[len]) & 0xC0u) == 0x80u)
			--len;

		l.name[len] = '\0';
		l.step[static_cast<uint32_t>(mstep::queued)].store(mnow(), std::memory_order_relaxed);
		nloads.store(id + 1u, std::memory_order_relaxed);
		return id;
	}

	
	uint32_t lenter(uint32_t id) noexcept {
		const uint32_t prev = curload;
		const uint32_t d = curdepth;

		if (id < NLOADS) {
			lrec& l = loads[id];
			l.step[static_cast<uint32_t>(mstep::started)].store(mnow(), std::memory_order_relaxed);
			l.depth.store(d, std::memory_order_relaxed);
		}

		curload = id < NLOADS ? id : NOLOAD;
		curdepth = d + 1u;
		return prev;
	}

	void lleave(uint32_t id, uint32_t prev) noexcept {
		if (id < NLOADS)
			loads[id].step[static_cast<uint32_t>(mstep::ended)].store(mnow(), std::memory_order_relaxed);

		curload = prev;
		curdepth = curdepth ? curdepth - 1u : 0u;
	}

	void lstep(mstep s) noexcept {
		const uint32_t id = curload;

		if (id >= NLOADS || s >= mstep::count)
			return;

		loads[id].step[static_cast<uint32_t>(s)].store(mnow(), std::memory_order_relaxed);
	}

	void linfo(uint32_t clips, uint32_t joints) noexcept {
		const uint32_t id = curload;

		if (id >= NLOADS)
			return;

		loads[id].clips.store(clips, std::memory_order_relaxed);
		loads[id].joints.store(joints, std::memory_order_relaxed);
	}

	
	void lspawn(uint32_t id) noexcept {
		if (id >= NLOADS)
			return;

		loads[id].step[static_cast<uint32_t>(mstep::spawned)].store(mnow(), std::memory_order_relaxed);
		mark(mmark::firstmodel);
	}

	static void sadd(std::atomic<uint64_t>& a, uint64_t d) noexcept {
		a.store(a.load(std::memory_order_relaxed) + d, std::memory_order_relaxed);
	}

	mwslot& slot() noexcept {
		mwslot* s = myslot;

		if (!s) [[unlikely]] {
			const uint32_t i = nslots.fetch_add(1u, std::memory_order_relaxed);
			s = &slots[i < NSLOTS ? i : NSLOTS];
			myslot = s;
		}

		return *s;
	}

	void wkind(uint32_t k) noexcept {
		slot().kind.store(k, std::memory_order_relaxed);
	}

	void jenq(bool low) noexcept {
		mwslot& s = slot();
		sadd(s.enq[low ? 1u : 0u], 1u);
	}

	void jrel(bool highonly) noexcept {
		trel[highonly ? 1u : 0u].store(mnow(), std::memory_order_relaxed);
	}

	uint64_t jbegin() noexcept {
		mwslot& s = slot();
		++s.depth;
		return mnow();
	}

	
	void jend(uint64_t t0, bool low) noexcept {
		const uint64_t d = mnow() - t0;
		mwslot& s = slot();
		const uint32_t p = low ? 1u : 0u;
		sadd(s.done[p], 1u);
		s.run[p].add(d);

		const uint32_t dp = s.depth;

		if (dp == 1u)
			sadd(s.busy, d);
		else if (dp == 2u)
			sadd(s.helped, d);

		if (dp >= 2u)
			sadd(s.helpedjobs, 1u);

		if (dp > s.maxdepth.load(std::memory_order_relaxed))
			s.maxdepth.store(dp, std::memory_order_relaxed);

		if (dp > 0)
			s.depth = dp - 1u;
	}

	
	uint64_t wjbegin() noexcept {
		return slot().depth == 1u ? mnow() : 0u;
	}

	void wjend(uint64_t t0) noexcept {
		if (t0 == 0)
			return;

		const uint64_t d = mnow() - t0;
		mwslot& s = slot();
		sadd(s.waited, d);
		sadd(s.waits, 1u);
	}

	void jwoke(uint64_t t0, bool highonly, bool running) noexcept {
		const uint64_t now = mnow();
		mwslot& s = slot();
		sadd(s.parks, 1u);
		sadd(s.parked, now - t0);

		const uint64_t tr = trel[highonly ? 1u : 0u].load(std::memory_order_relaxed);

		if (running && tr >= t0 && tr <= now)
			s.wake.add(now - tr);
	}

	void janim(uint64_t wait, uint64_t run) noexcept {
		const uint32_t p = curphase.load(std::memory_order_relaxed) ? 1u : 0u;
		mwslot& s = slot();
		s.animwait[p].add(wait);
		s.animrun[p].add(run);
	}

	
	void jsample(uint32_t qhigh, uint32_t qlow, uint32_t idle, uint32_t hidle, uint64_t nfull) noexcept {
		qdepth[0].add(qhigh);
		qdepth[1].add(qlow);
		idlesum += idle;
		hidlesum += hidle;
		++samples;
		qfull = nfull;
		TracyPlot("jobs queued high", static_cast<int64_t>(qhigh));
		TracyPlot("jobs queued low", static_cast<int64_t>(qlow));
		TracyPlot("workers parked", static_cast<int64_t>(idle) + static_cast<int64_t>(hidle));
	}

	
	void bargs(int argc, char** argv) noexcept {
		bmode = mbench::none;
		bsecs = 0.0;
		bnc = false;

		if (!argv)
			return;

		bool hassec = false;

		for (int i = 1; i < argc; ++i) {
			const char* a = argv[i];

			if (!a)
				continue;

			if (std::strcmp(a, "--bench") == 0 && i + 1 < argc && argv[i + 1]) {
				if (std::strcmp(argv[i + 1], "boot") == 0) {
					bmode = mbench::boot;
					++i;
				} else if (std::strcmp(argv[i + 1], "quit") == 0) {
					bmode = mbench::quit;
					++i;
				}
			} else if (std::strcmp(a, "--seconds") == 0 && i + 1 < argc && argv[i + 1]) {
				char* end = nullptr;
				const double v = std::strtod(argv[i + 1], &end);

				if (end != argv[i + 1] && *end == '\0' && std::isfinite(v) && v >= 0.0) {
					bsecs = v;
					hassec = true;
					++i;
				}
			} else if (std::strcmp(a, "--nocap") == 0) {
				bnc = true;
			}
		}

		if (bmode == mbench::none)
			bsecs = 0.0;
		else if (!hassec)
			bsecs = bmode == mbench::boot ? 10.0 : 2.0;
	}

	bool bnocap() const noexcept {
		return bnc;
	}

	
	bool bquit() const noexcept {
		if (bmode == mbench::none)
			return false;

		const mmark m = bmode == mbench::boot ? mmark::allboot : mmark::firstframe;
		const uint64_t t0 = marks[static_cast<uint32_t>(m)].load(std::memory_order_relaxed);

		if (t0 == 0)
			return false;

		const uint64_t now = mnow();
		return now >= t0 && mms(now - t0) >= bsecs * 1000.0;
	}

	
	void report() noexcept {
		const long long t = static_cast<long long>(std::time(nullptr));
		const char* mode = bmode == mbench::none ? "run" : mbenchname[static_cast<uint32_t>(bmode)];
		char path[128];

		(void)SDL_CreateDirectory("perf"); 

		std::snprintf(path, sizeof(path), "perf/%s-%s-%lld.json", mode, RVIEW_CONFIG, t);

		if (std::FILE* f = std::fopen(path, "wb")) {
			wjson(f);
			std::fclose(f); 
			SDL_Log("%s", path);
		}

		std::snprintf(path, sizeof(path), "perf/%s-%s-%lld.frames.csv", mode, RVIEW_CONFIG, t);

		if (std::FILE* f = std::fopen(path, "wb")) {
			wcsv(f);
			std::fclose(f);
		}
	}

	static uint32_t clamp32(uint64_t v) noexcept {
		return v > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(v);
	}

	
	void wput(uint32_t i, mphase ph, uint64_t iv) noexcept {
		wrec& w = worst[i];
		w.frame = nframes;
		w.work = work;
		w.interval = iv;
		w.phase = ph;
		w.quiet = fq;

		for (uint32_t k = 0; k < NST; ++k)
			w.st[k] = st[k];
	}

	
	void wfind() noexcept {
		wlo = 0;

		for (uint32_t i = 1; i < NWORST; ++i)
			if (worst[i].work < worst[wlo].work)
				wlo = i;
	}

	
	static uint64_t mticks(double ms) noexcept {
		return static_cast<uint64_t>(ms / mms(1));
	}

	
	static void wms(std::FILE* f, const char* pre, double ms) noexcept {
		const unsigned long long v = static_cast<unsigned long long>(ms * 10000.0 + 0.5);
		std::fprintf(f, "%s%llu.%04llu", pre, v / 10000u, v % 10000u);
	}

	
	static void wnum(std::FILE* f, const char* pre, double x) noexcept {
		const double c = x > 1e9 ? 1e9 : (x < 0.0 ? 0.0 : x);
		const unsigned long long v = static_cast<unsigned long long>(c * 10000.0 + 0.5);
		unsigned long long frac = v % 10000u;

		if (frac == 0) {
			std::fprintf(f, "%s%llu", pre, v / 10000u);
			return;
		}

		int d = 4;

		while (frac % 10u == 0) {
			frac /= 10u;
			--d;
		}

		std::fprintf(f, "%s%llu.%0*llu", pre, v / 10000u, d, frac);
	}

	
	static void whist(std::FILE* f, const mhist& h, bool full, bool over) noexcept {
		wms(f, "{ \"mean\": ", h.n ? mms(h.sum) / static_cast<double>(h.n) : 0.0);
		wms(f, ", \"p50\": ", mms(h.pct(0.50)));

		if (full)
			wms(f, ", \"p90\": ", mms(h.pct(0.90)));

		wms(f, ", \"p99\": ", mms(h.pct(0.99)));

		if (full)
			wms(f, ", \"p999\": ", mms(h.pct(0.999)));

		wms(f, ", \"max\": ", mms(h.hi));

		if (over)
			std::fprintf(f, ", \"over_8_33\": %llu, \"over_16_67\": %llu, \"over_33_33\": %llu, \"over_100\": %llu",
				static_cast<unsigned long long>(h.above(mticks(8.33))),
				static_cast<unsigned long long>(h.above(mticks(16.67))),
				static_cast<unsigned long long>(h.above(mticks(33.33))),
				static_cast<unsigned long long>(h.above(mticks(100.0))));

		std::fprintf(f, " }");
	}

	
	static uint64_t mdiff(uint64_t a, uint64_t b) noexcept {
		return (a == 0 || b == 0 || b < a) ? UINT64_MAX : b - a;
	}

	static void wmsopt(std::FILE* f, const char* pre, uint64_t d) noexcept {
		if (d == UINT64_MAX)
			std::fprintf(f, "%s-1", pre);
		else
			wms(f, pre, mms(d));
	}

	static void wstr(std::FILE* f, const char* s) noexcept {
		std::fputc('"', f);

		for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
			if (*p == '"' || *p == '\\')
				std::fputc('\\', f);

			if (*p >= 0x20u)
				std::fputc(static_cast<int>(*p), f);
		}

		std::fputc('"', f);
	}

	static void wlsum(std::FILE* f, uint64_t* a, uint32_t n) noexcept {
		if (n == 0) {
			std::fprintf(f, "{ \"p50\": -1, \"max\": -1 }");
			return;
		}

		for (uint32_t i = 1; i < n; ++i) {
			const uint64_t v = a[i];
			uint32_t k = i;

			for (; k > 0 && a[k - 1u] > v; --k)
				a[k] = a[k - 1u];

			a[k] = v;
		}

		wms(f, "{ \"p50\": ", mms(a[(n - 1u) / 2u]));
		wms(f, ", \"max\": ", mms(a[n - 1u]));
		std::fprintf(f, " }");
	}

	void wtimeline(std::FILE* f) const noexcept {
		const uint64_t t0 = marks[static_cast<uint32_t>(mmark::mainentry)].load(std::memory_order_relaxed);
		std::fprintf(f, "  \"timeline_ms\": {");

		if (procms < 0.0)
			std::fprintf(f, " \"process_to_main\": -1");
		else
			wms(f, " \"process_to_main\": ", procms);

		for (uint32_t i = 1; i < NMARK; ++i) {
			const uint64_t t = marks[i].load(std::memory_order_relaxed);
			std::fprintf(f, ", \"%s\": ", mmarkname[i]);
			wmsopt(f, "", mdiff(t0, t));
		}

		const uint64_t tq = marks[static_cast<uint32_t>(mmark::quit)].load(std::memory_order_relaxed);
		const uint64_t tj = marks[static_cast<uint32_t>(mmark::joined)].load(std::memory_order_relaxed);
		const uint64_t tc = marks[static_cast<uint32_t>(mmark::cleaned)].load(std::memory_order_relaxed);

		std::fprintf(f, " },\n  \"exit_ms\": {");
		wmsopt(f, " \"join\": ", mdiff(tq, tj));
		wmsopt(f, ", \"total\": ", mdiff(tq, tc));
		std::fprintf(f, " },\n");
	}

	void wloads(std::FILE* f) const noexcept {
		const uint64_t t0 = marks[static_cast<uint32_t>(mmark::mainentry)].load(std::memory_order_relaxed);
		uint32_t n = nloads.load(std::memory_order_relaxed);

		if (n > NLOADS)
			n = NLOADS;

		uint64_t waits[NLOADS];
		uint64_t jobs[NLOADS];
		uint64_t viss[NLOADS];
		uint32_t nwait = 0, njob = 0, nvis = 0, nspawn = 0;

		std::fprintf(f, "  \"loads\": [");

		for (uint32_t i = 0; i < n; ++i) {
			const lrec& l = loads[i];
			const uint64_t tq = l.step[static_cast<uint32_t>(mstep::queued)].load(std::memory_order_relaxed);
			const uint64_t ts = l.step[static_cast<uint32_t>(mstep::started)].load(std::memory_order_relaxed);
			const uint64_t tbb = l.step[static_cast<uint32_t>(mstep::bakebegin)].load(std::memory_order_relaxed);
			const uint64_t tbe = l.step[static_cast<uint32_t>(mstep::bakeend)].load(std::memory_order_relaxed);
			const uint64_t tp = l.step[static_cast<uint32_t>(mstep::parsed)].load(std::memory_order_relaxed);
			const uint64_t tc = l.step[static_cast<uint32_t>(mstep::committed)].load(std::memory_order_relaxed);
			const uint64_t tpu = l.step[static_cast<uint32_t>(mstep::pushed)].load(std::memory_order_relaxed);
			const uint64_t te = l.step[static_cast<uint32_t>(mstep::ended)].load(std::memory_order_relaxed);
			const uint64_t tsp = l.step[static_cast<uint32_t>(mstep::spawned)].load(std::memory_order_relaxed);

			const uint64_t dq = mdiff(t0, tq);
			const uint64_t dw = mdiff(tq, ts);
			const uint64_t dp = mdiff(ts, tp);
			const uint64_t db = mdiff(tbb, tbe);
			const uint64_t dc = mdiff(tp, tc);
			const uint64_t dpu = mdiff(tc, tpu);
			const uint64_t dj = mdiff(ts, te);
			const uint64_t dv = mdiff(tq, tsp);

			if (tsp != 0)
				++nspawn;

			if (dw != UINT64_MAX)
				waits[nwait++] = dw;

			if (dj != UINT64_MAX)
				jobs[njob++] = dj;

			if (dv != UINT64_MAX)
				viss[nvis++] = dv;

			std::fprintf(f, "%s\n    { \"id\": %u, \"name\": ", i ? "," : "", i);
			wstr(f, l.name);
			std::fprintf(f, ", \"depth\": %u, \"clips\": %u, \"joints\": %u",
				l.depth.load(std::memory_order_relaxed),
				l.clips.load(std::memory_order_relaxed),
				l.joints.load(std::memory_order_relaxed));
			wmsopt(f, ", \"queued_ms\": ", dq);
			wmsopt(f, ", \"wait_ms\": ", dw);
			wmsopt(f, ", \"parse_ms\": ", dp);
			wmsopt(f, ", \"bake_ms\": ", db);
			wmsopt(f, ", \"commit_ms\": ", dc);
			wmsopt(f, ", \"push_ms\": ", dpu);
			wmsopt(f, ", \"job_ms\": ", dj);
			wmsopt(f, ", \"visible_ms\": ", dv);
			std::fprintf(f, " }");
		}

		std::fprintf(f, "%s  \"loads_summary\": { \"n\": %u, \"spawned\": %u, \"wait_ms\": ", n ? "\n  ],\n" : "],\n", n, nspawn);
		wlsum(f, waits, nwait);
		std::fprintf(f, ", \"job_ms\": ");
		wlsum(f, jobs, njob);
std::fprintf(f, ", \"visible_ms\": ");
		wlsum(f, viss, nvis);
		std::fprintf(f, " },\n");
	}

	static void wjhist(std::FILE* f, const mhist& h) noexcept {
		std::fprintf(f, "{ \"n\": %llu", static_cast<unsigned long long>(h.n));
		wms(f, ", \"mean\": ", h.n ? mms(h.sum) / static_cast<double>(h.n) : 0.0);
		wms(f, ", \"p50\": ", mms(h.pct(0.50)));
		wms(f, ", \"p99\": ", mms(h.pct(0.99)));
		wms(f, ", \"max\": ", mms(h.hi));
		std::fprintf(f, " }");
	}

	static void wqhist(std::FILE* f, const mhist& h) noexcept {
		std::fprintf(f, "{ \"p50\": %llu, \"p99\": %llu, \"max\": %llu }",
			static_cast<unsigned long long>(h.pct(0.50)),
			static_cast<unsigned long long>(h.pct(0.99)),
			static_cast<unsigned long long>(h.hi));
	}

	void wjobs(std::FILE* f) const noexcept {
		const uint32_t n = nslots.load(std::memory_order_relaxed);
		const uint32_t used = n > NSLOTS ? NSLOTS + 1u : n;

		uint64_t enqh = 0, enql = 0;

		for (uint32_t i = 0; i < used; ++i) {
			enqh += slots[i].enq[0].load(std::memory_order_relaxed);
			enql += slots[i].enq[1].load(std::memory_order_relaxed);
		}

		static mhist h;

		std::fprintf(f, "  \"jobs\": {\n    \"enqueued\": { \"high\": %llu, \"low\": %llu }, \"full\": %llu,\n    \"run_ms\": { \"high\": ",
			static_cast<unsigned long long>(enqh),
			static_cast<unsigned long long>(enql),
			static_cast<unsigned long long>(qfull));

		h.clear();
		for (uint32_t i = 0; i < used; ++i)
			h.merge(slots[i].run[0]);
		wjhist(f, h);

		std::fprintf(f, ", \"low\": ");
		h.clear();
		for (uint32_t i = 0; i < used; ++i)
			h.merge(slots[i].run[1]);
		wjhist(f, h);

		std::fprintf(f, " },\n    \"wake_ms\": ");
		h.clear();
		for (uint32_t i = 0; i < used; ++i)
			h.merge(slots[i].wake);
		wjhist(f, h);

		std::fprintf(f, ",\n    \"anim\": {");

		for (uint32_t p = 0; p < NPH; ++p) {
			std::fprintf(f, "%s \"%s\": { \"wait_ms\": ", p ? "," : "", mphasename[p]);
			h.clear();
			for (uint32_t i = 0; i < used; ++i)
				h.merge(slots[i].animwait[p]);
			wjhist(f, h);

			std::fprintf(f, ", \"run_ms\": ");
			h.clear();
			for (uint32_t i = 0; i < used; ++i)
				h.merge(slots[i].animrun[p]);
			wjhist(f, h);
			std::fprintf(f, " }");
		}

		std::fprintf(f, " },\n    \"queue_depth\": { \"high\": ");
		wqhist(f, qdepth[0]);
		std::fprintf(f, ", \"low\": ");
		wqhist(f, qdepth[1]);

		const double gmean = samples ? static_cast<double>(idlesum) / static_cast<double>(samples) : 0.0;
		const double hmean = samples ? static_cast<double>(hidlesum) / static_cast<double>(samples) : 0.0;

		std::fprintf(f, " },\n    \"parked_mean\": { ");
		wms(f, "\"general\": ", gmean);
		wms(f, ", \"highonly\": ", hmean);
		std::fprintf(f, " },\n    \"threads\": [");

		for (uint32_t i = 0; i < used; ++i) {
			const mwslot& s = slots[i];
			const uint64_t k = s.kind.load(std::memory_order_relaxed);
			const char* ks = k < 4u ? mkindname[k] : mkindname[0];

			std::fprintf(f, "%s\n      { \"slot\": %u, \"kind\": \"%s\", \"high\": %llu, \"low\": %llu",
				i ? "," : "", i, ks,
				static_cast<unsigned long long>(s.done[0].load(std::memory_order_relaxed)),
				static_cast<unsigned long long>(s.done[1].load(std::memory_order_relaxed)));
			wms(f, ", \"busy_ms\": ", mms(s.busy.load(std::memory_order_relaxed)));
			wms(f, ", \"helped_ms\": ", mms(s.helped.load(std::memory_order_relaxed)));
			std::fprintf(f, ", \"helped_jobs\": %llu", static_cast<unsigned long long>(s.helpedjobs.load(std::memory_order_relaxed)));
			wms(f, ", \"waited_ms\": ", mms(s.waited.load(std::memory_order_relaxed)));
			std::fprintf(f, ", \"waits\": %llu, \"parks\": %llu",
				static_cast<unsigned long long>(s.waits.load(std::memory_order_relaxed)),
				static_cast<unsigned long long>(s.parks.load(std::memory_order_relaxed)));
			wms(f, ", \"parked_ms\": ", mms(s.parked.load(std::memory_order_relaxed)));
			std::fprintf(f, ", \"maxdepth\": %llu }", static_cast<unsigned long long>(s.maxdepth.load(std::memory_order_relaxed)));
		}

		std::fprintf(f, "%s  },\n", used ? "\n    ]\n" : "]\n");
	}

	static void wghist(std::FILE* f, const mhist& h) noexcept {
		std::fprintf(f, "{ \"n\": %llu", static_cast<unsigned long long>(h.n));
		wms(f, ", \"mean\": ", h.n ? static_cast<double>(h.sum) / (static_cast<double>(h.n) * 1e6) : 0.0);
		wms(f, ", \"p50\": ", static_cast<double>(h.pct(0.50)) / 1e6);
		wms(f, ", \"p99\": ", static_cast<double>(h.pct(0.99)) / 1e6);
		wms(f, ", \"max\": ", static_cast<double>(h.hi) / 1e6);
		std::fprintf(f, " }");
	}

	static double mmb(uint64_t b) noexcept {
		return static_cast<double>(b) / (1024.0 * 1024.0);
	}

	void wgpu(std::FILE* f) const noexcept {
		std::fprintf(f, "  \"gpu_ms\": {");

		for (uint32_t p = 0; p < NPH; ++p) {
			std::fprintf(f, "%s \"%s\": ", p ? "," : "", mphasename[p]);
			wghist(f, ps[p].gpu);
		}

		std::fprintf(f, " },\n");
	}

	void wmem(std::FILE* f) const noexcept {
		std::fprintf(f, "  \"memory\": { \"working_set_mb\": {");
		wnum(f, " \"last\": ", mmb(wlast));
		wnum(f, ", \"peak\": ", mmb(wpeak));
		std::fprintf(f, " }, \"private_mb\": {");
		wnum(f, " \"last\": ", mmb(plast));
		wnum(f, ", \"peak\": ", mmb(ppeak));
		std::fprintf(f, " }, \"vk_used_mb\": {");
		wnum(f, " \"last\": ", mmb(vused));
		wnum(f, ", \"peak\": ", mmb(vpeak));
		wnum(f, " }, \"vk_budget_mb\": ", mmb(vbud));
		std::fprintf(f, " },\n");
	}

	void wjson(std::FILE* f) const noexcept {
		std::fprintf(f, "{\n  \"schema\": 5,\n  \"build\": { \"config\": \"%s\", \"demo\": %d },\n  \"bench\": { \"mode\": \"%s\"",
			RVIEW_CONFIG, mdemo, mbenchname[static_cast<uint32_t>(bmode)]);
		wnum(f, ", \"seconds\": ", bsecs);
		std::fprintf(f, ", \"nocap\": %d },\n", bnc ? 1 : 0);
		wtimeline(f);
		wloads(f);
		wjobs(f);
		wgpu(f);
		wmem(f);
		std::fprintf(f, "  \"frames\": {\n");

		for (uint32_t p = 0; p < NPH; ++p) {
			const pstat& s = ps[p];
			
			const unsigned long long q = s.frames ? static_cast<unsigned long long>((s.quietframes * 10000u + s.frames / 2u) / s.frames) : 0u;

			std::fprintf(f, "    \"%s\": {\n      \"n\": %llu, \"quiet_pct\": %llu.%02llu,\n      \"work_ms\": ", mphasename[p], static_cast<unsigned long long>(s.frames), q / 100u, q % 100u);
			whist(f, s.work, true, false);
			std::fprintf(f, ",\n      \"interval_ms\": ");
			whist(f, s.interval, true, true);
			std::fprintf(f, ",\n      \"stages_ms\": {\n");

			for (uint32_t i = 0; i < NST; ++i) {
				std::fprintf(f, "        \"%s\": ", mstagename[i]);
				whist(f, s.stage[i], false, false);
				std::fprintf(f, "%s\n", i + 1u < NST ? "," : "");
			}

			std::fprintf(f, "      }\n    }%s\n", p + 1u < NPH ? "," : "");
		}

		std::fprintf(f, "  },\n  \"worst\": [");

		
		uint32_t ord[NWORST];

		for (uint32_t i = 0; i < nworst; ++i) {
			uint32_t k = i;

			for (; k > 0; --k) {
				const wrec& a = worst[ord[k - 1u]];
				const wrec& c = worst[i];

				if (a.work > c.work || (a.work == c.work && a.frame < c.frame))
					break;

				ord[k] = ord[k - 1u];
			}

			ord[k] = i;
		}

		for (uint32_t i = 0; i < nworst; ++i) {
			const wrec& w = worst[ord[i]];

			std::fprintf(f, "%s\n    { \"frame\": %llu, \"phase\": \"%s\", \"quiet\": %d", i ? "," : "", static_cast<unsigned long long>(w.frame), mphasename[static_cast<uint32_t>(w.phase)], w.quiet ? 1 : 0);
			wms(f, ", \"work_ms\": ", mms(w.work));
			wms(f, ", \"interval_ms\": ", mms(w.interval));
			std::fprintf(f, ", \"stages_ms\": {");

			
			bool any = false;

			for (uint32_t k = 0; k < NST; ++k) {
				const double ms = mms(w.st[k]);

				if (ms < 0.05)
					continue;

				std::fprintf(f, "%s \"%s\": ", any ? "," : "", mstagename[k]);
				wms(f, "", ms);
				any = true;
			}

			std::fprintf(f, "%s", any ? " } }" : "} }");
		}

		std::fprintf(f, "%s", nworst ? "\n  ]\n}\n" : "]\n}\n");
	}

	
	void wcsv(std::FILE* f) const noexcept {
		std::fprintf(f, "frame,phase,work_ms,interval_ms\n");

		for (uint64_t i = nframes > NLOG ? nframes - NLOG : 0; i < nframes; ++i) {
			const frec& r = flog[i & (NLOG - 1u)];

			std::fprintf(f, "%llu,%s", static_cast<unsigned long long>(i), mphasename[static_cast<uint32_t>(r.phase)]);
			wms(f, ",", mms(r.work));
			wms(f, ",", mms(r.interval));
			std::fprintf(f, "\n");
		}
	}
};

inline constinit metrics gm;

inline double mms(uint64_t t) noexcept {
	if (gm.freq == 0) {
		const uint64_t f = SDL_GetPerformanceFrequency();
		gm.freq = f ? f : 1; 
	}

	return static_cast<double>(t) * 1000.0 / static_cast<double>(gm.freq);
}

#else

// metrics off

inline uint64_t mnow() noexcept {
	return 0;
}

inline double mms(uint64_t) noexcept {
	return 0.0;
}

struct metrics {
	void fbegin() noexcept {}
	void stage(mstage) noexcept {}
	void quiet(bool) noexcept {}
	void uipass(uint64_t) noexcept {}
	void fclose() noexcept {}
	void fend(bool) noexcept {}
	void gpu(uint64_t) noexcept {}
	void vmem(uint64_t, uint64_t) noexcept {}
	void report() noexcept {}
	void mark(mmark) noexcept {}
	uint32_t lnew(const char*) noexcept { return 0xFFFFFFFFu; }
	void lstep(mstep) noexcept {}
	void linfo(uint32_t, uint32_t) noexcept {}
	void lspawn(uint32_t) noexcept {}
	void wkind(uint32_t) noexcept {}
	void jenq(bool) noexcept {}
	void jrel(bool) noexcept {}
	uint64_t jbegin() noexcept { return 0; }
	void jend(uint64_t, bool) noexcept {}
	uint64_t wjbegin() noexcept { return 0; }
	void wjend(uint64_t) noexcept {}
	void jwoke(uint64_t, bool, bool) noexcept {}
	void janim(uint64_t, uint64_t) noexcept {}
	void jsample(uint32_t, uint32_t, uint32_t, uint32_t, uint64_t) noexcept {}
	void bargs(int, char**) noexcept {}
	bool bnocap() const noexcept { return false; }
	bool bquit() const noexcept { return false; }
};

inline constinit metrics gm;

#endif

struct mscope {
	mscope() noexcept { gm.fbegin(); }
	~mscope() noexcept { gm.fclose(); }
};

struct mloadscope {
#if RVIEW_METRICS
	uint32_t id;
	uint32_t prev;
	mloadscope(uint32_t i) noexcept : id(i), prev(gm.lenter(i)) {}
	~mloadscope() noexcept { gm.lleave(id, prev); }
#else
	mloadscope(uint32_t) noexcept {}
	~mloadscope() noexcept {}
#endif
	mloadscope(const mloadscope&) = delete;
	mloadscope& operator=(const mloadscope&) = delete;
};