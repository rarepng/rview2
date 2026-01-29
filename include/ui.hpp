#pragma once
#include <vector>

#include "modelsettings.hpp"
#include "core/rvk.hpp"

enum struct ppick { rock, paper, scissor };
enum struct gstate { tie, ai, player };

struct selection {
	std::vector<std::vector<modelsettings *>> instancesettings{};
	size_t midx{0};
	size_t iidx{0};
};

namespace ui {
bool init(rvkbucket &mvkobjs);
void createdbgframe(rvkbucket &mvkobjs, selection &settingsz);
bool createloadingscreen(rvkbucket &mvkobjs);
bool createpausebuttons(rvkbucket &mvkobjs);
void addchat(std::string s);
void render(rvkbucket &mvkobjs, VkCommandBuffer &cbuffer);
void cleanup(rvkbucket &mvkobjs);
void backspace();

static bool setnetwork{false};
static bool chatfocus{false};

static std::string inputxt{};
static std::vector<std::string> chattxts;

static ppick mpick;
static ppick aipick;
static gstate mstate;

static unsigned int nframes{0};

static bool aipicking{false};

static int selectednetwork{0};

static bool offline{true};
static bool hosting{false};
static bool connectingtohost{false};

static float mfps = 0.0f;
static float mavgalpha = 0.96f;
static std::vector<float> mfpsvalues{};
static int mnumfpsvalues = 90;
static std::vector<float> mframetimevalues{};
static int mnumframetimevalues = 90;
static std::vector<float> mmodeluploadvalues{};
static int mnummodeluploadvalues = 90;
static std::vector<float> mmatrixgenvalues{};
static int mnummatrixgenvalues = 90;
static std::vector<float> mikvalues{};
static int mnumikvalues = 90;
static std::vector<float> mmatrixuploadvalues{};
static int mnummatrixuploadvalues = 90;
static std::vector<float> muigenvalues{};
static int mnumuigenvalues = 90;
static std::vector<float> mmuidrawvalues{};
static int mnummuidrawvalues = 90;
};
