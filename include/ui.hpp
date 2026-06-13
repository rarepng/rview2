#pragma once
#include <vector>

#include "core/rvk.hpp"
#include <meta>

template<size_t Size = 90>
struct PerfGraph {
	std::array<float, Size> data{};
	int offset = 0;

	void push(float val) {
		data[offset] = val;
		offset = (offset + 1) % Size;
	}
	float get_avg() const {
		float sum = 0.0f;

		for (float v : data) sum += v;

		return sum / Size;
	}
};

namespace ui {
inline PerfGraph<> fps_graph, frametime_graph, vbo_graph, matgen_graph, ik_graph, ubo_graph, uigen_graph, uidraw_graph;
bool init(rvkbucket &mvkobjs);
void createdbgframe(rvkbucket &mvkobjs);
bool createloadingscreen(rvkbucket &mvkobjs);
bool createpausebuttons(rvkbucket &mvkobjs);
void addchat(std::string s);
void render(rvkbucket &mvkobjs, VkCommandBuffer cbuffer);
void cleanup(rvkbucket &mvkobjs);
void backspace();

inline std::string inputxt{};
inline std::vector<std::string> chattxts;
inline float mfps = 0.0f;
};
