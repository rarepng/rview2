#include "timer.hpp"

void timer::start() {
	if (mstate) {
		return;
	}
	mstate = true;
	mstart = std::chrono::steady_clock::now();
}

float timer::stop() {
	if (!mstate) {
		return 0;
	}
	mstate = false;
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-mstart).count()/1000.0f;
}
