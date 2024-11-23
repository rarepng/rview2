#pragma once
#include <cstdio>
class logger {
public:
	template <typename... Args>
	static void log(unsigned int lvl, Args ... args) {
		if (lvl <= mlvl) {
			std::printf(args ...);
			std::fflush(stdout);
		}
	}
	static void setlvl(unsigned int inlvl) {
		inlvl <= 9 ? mlvl = inlvl : mlvl = 9;
	}

private:
	inline static unsigned int mlvl{ 1 };
};