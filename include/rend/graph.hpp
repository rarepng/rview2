#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <new>
#include <tracy/Tracy.hpp>

class alignas(64) RenderGraph {
public:
	// 1MB [[heap]] but only once at startup ((i hope))
	static constexpr size_t MEMORY_CAPACITY = 1024 * 1024;
	static constexpr size_t MAX_PASSES = 256;

	RenderGraph() {
		// buffer = static_cast<char*>(_aligned_malloc(MEMORY_CAPACITY, 64));
		// mingw :(
		// buffer = new (std::align_val_t{64}) char[MEMORY_CAPACITY];
		buffer = new char[MEMORY_CAPACITY];
	}

	~RenderGraph() {
		clear();
		// _aligned_free(buffer);
		// mingw x2 :(
		// ::operator delete[](buffer, std::align_val_t{64});
		delete[] buffer;
	}

	RenderGraph(const RenderGraph&) = delete;
	RenderGraph& operator=(const RenderGraph&) = delete;

	template<typename F>
	void add_pass(F&& f) {
		using DecayedF = std::decay_t<F>;

		void* ptr = buffer + offset;
		size_t space = MEMORY_CAPACITY - offset;

		void* aligned_ptr = std::align(alignof(DecayedF), sizeof(DecayedF), ptr, space);

		if (!aligned_ptr || pass_count >= MAX_PASSES) return;

		offset = MEMORY_CAPACITY - space + sizeof(DecayedF);

		new (aligned_ptr) DecayedF(std::forward<F>(f));

		PassRecord& record = passes[pass_count++];
		record.payload = aligned_ptr;

		record.execute = [](void* data) {
			(*static_cast<DecayedF*>(data))();
		};

		if constexpr (!std::is_trivially_destructible_v<DecayedF>) {
			record.destroy = [](void* data) {
				static_cast<DecayedF*>(data)->~DecayedF();
			};
		} else {
			record.destroy = nullptr;
		}
	}

	inline void execute() {
		ZoneScoped;

		for (uint32_t i = 0; i < pass_count; ++i) {
			ZoneScopedN("pass");
			passes[i].execute(passes[i].payload);
		}
	}

	inline void clear() {
		for (uint32_t i = pass_count; i-- > 0;) {
			if (passes[i].destroy) {
				passes[i].destroy(passes[i].payload);
			}
		}

		pass_count = 0;
		offset = 0;
	}

private:
	struct PassRecord {
		void (*execute)(void*);
		void (*destroy)(void*);
		void* payload;
	};

	char* buffer{nullptr};
	size_t offset{0};

	alignas(64) PassRecord passes[MAX_PASSES] {};
	uint32_t pass_count{0};
};