#pragma once
#include <chrono>

class timer {
public:
	inline void start() {
		if (mstate) return;
		mstate = true;
		mstart = std::chrono::steady_clock::now();
	}

	inline float stop() {
		if (!mstate) return 0.0f;
		mstate = false;
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - mstart).count() / 1000.0f;
	}

private:
	bool mstate = false;
	std::chrono::time_point<std::chrono::steady_clock> mstart{};
};