#pragma once
#include <vector>



#include "vkobjs.hpp"
#include "modelsettings.hpp"


class ui {
public:
	bool init(vkobjs& mvkobjs);
	void createframe(vkobjs& mvkobjs, modelsettings& settings);
	bool createmainmenuframe(vkobjs& mvkobjs);
	bool createloadingscreen(vkobjs& mvkobjs);
	void render(vkobjs& mvkobjs,VkCommandBuffer& cbuffer);
	void cleanup(vkobjs& mvkobjs);
private:
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