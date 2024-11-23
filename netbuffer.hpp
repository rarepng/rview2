#pragma once
#define _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR

#include <memory>
#include <string>
#include <cstring>

struct netbuffer {
	void* d;
	uint64_t s;

	netbuffer() :d(nullptr), s(0) {};
	netbuffer(const void* d0, uint64_t s0) :d((void*)d0), s(s0) {};
	static netbuffer copy(const netbuffer& other) {
		netbuffer b;
		b.alloc(other.s);
		memcpy(b.d, other.d, other.s);
		return b;
	}
	void alloc(uint64_t s0) {
		delete[](uint8_t*)d;
		d = nullptr;
		if (s0 == 0)return;
		d = new uint8_t[s0];
		s = s0;
	}
	void release() {
		delete[](uint8_t*)d;
		d = nullptr;
		s = 0;
	}
	void zeroinit() {
		if (d)memset(d, 0, s);
	}
	template<typename T>
	T& read(uint64_t ofx = 0) {
		return *(T*)((uint32_t*)d + ofx);
	}
	template<typename T>
	T& read(uint64_t ofx = 0) const {
		return *(T*)((uint32_t*)d + ofx);
	}
	uint8_t* readbytes(uint64_t s0, uint64_t ofx) const {
		uint8_t* b = new uint8_t[s0];
		memcpy(b, (uint8_t*)d + ofx, s0);
		return b;
	}
	void write(const void* d0, uint64_t s0, uint64_t ofx = 0) {
		memcpy((uint8_t*)d + ofx, d0, s0);
	}
	operator bool() const {
		return d;
	}
	uint8_t& operator[](int idx) {
		return ((uint8_t*)d)[idx];
	}
	uint8_t operator[](int idx)const {
		return ((uint8_t*)d)[idx];
	}
	template<typename T>
	T* as() const {
		return (T*)d;
	}
	inline uint64_t getsize() const { return s; }
};
