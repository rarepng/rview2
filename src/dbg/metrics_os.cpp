#include "dbg/metrics.hpp"

// AAAAAAAAAAAAAAAAAAAAAAAAAAA!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#endif

bool mos_mem(uint64_t& workingset, uint64_t& privatebytes) noexcept {
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS_EX pmc{};
	pmc.cb = sizeof(pmc);

	if (!K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
		return false;

	workingset = static_cast<uint64_t>(pmc.WorkingSetSize);
	privatebytes = static_cast<uint64_t>(pmc.PrivateUsage);
	return true;
#else
	(void)workingset;
	(void)privatebytes;
	return false;
#endif
}

double mos_startms() noexcept {
#ifdef _WIN32
	FILETIME create{}, exit{}, kernel{}, user{}, now{};

	if (!GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user))
		return -1.0;

	GetSystemTimeAsFileTime(&now);

	const uint64_t t0 = (static_cast<uint64_t>(create.dwHighDateTime) << 32) | static_cast<uint64_t>(create.dwLowDateTime);
	const uint64_t t1 = (static_cast<uint64_t>(now.dwHighDateTime) << 32) | static_cast<uint64_t>(now.dwLowDateTime);

	if (t1 < t0)
		return -1.0;

	return static_cast<double>(t1 - t0) / 10000.0;
#else
	return -1.0;
#endif
}
