#pragma once
#include <chrono>

class timer {
public:
	void start();
	float stop();
private:
	bool mstate = false;
	std::chrono::time_point<std::chrono::steady_clock> mstart{};
};