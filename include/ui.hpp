#pragma once
#include <vector>

#include "modelsettings.hpp"
#include "vkobjs.hpp"

enum struct ppick { rock, paper, scissor };
enum struct gstate { tie, ai, player };

struct selection {
	std::vector<std::vector<modelsettings *>> instancesettings{};
	size_t midx{0};
	size_t iidx{0};
};

class ui {
public:
	bool init(rvk &mvkobjs);
	void createdbgframe(rvk &mvkobjs, selection &settingsz);
	bool createloadingscreen(rvk &mvkobjs);
	bool createpausebuttons(rvk &mvkobjs);
	void addchat(std::string s);
	void render(rvk &mvkobjs, VkCommandBuffer &cbuffer);
	void cleanup(rvk &mvkobjs);
	bool setnetwork{false};
	void backspace();
	bool chatfocus{false};
	std::vector<int> playerwave;

private:
	std::string inputxt{};
	std::vector<std::string> chattxts;

	// unsigned int playergold;

	ppick mpick;
	ppick aipick;
	gstate mstate;

	unsigned int nframes{0};

	bool aipicking{false};

	int selectednetwork{0};

	bool offline{true};
	bool hosting{false};
	bool connectingtohost{false};

	float mfps = 0.0f;
	float mavgalpha = 0.96f;
	std::vector<float> mfpsvalues{};
	int mnumfpsvalues = 90;
	std::vector<float> mframetimevalues{};
	int mnumframetimevalues = 90;
	std::vector<float> mmodeluploadvalues{};
	int mnummodeluploadvalues = 90;
	std::vector<float> mmatrixgenvalues{};
	int mnummatrixgenvalues = 90;
	std::vector<float> mikvalues{};
	int mnumikvalues = 90;
	std::vector<float> mmatrixuploadvalues{};
	int mnummatrixuploadvalues = 90;
	std::vector<float> muigenvalues{};
	int mnumuigenvalues = 90;
	std::vector<float> mmuidrawvalues{};
	int mnummuidrawvalues = 90;
};
