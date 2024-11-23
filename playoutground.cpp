#include "playoutground.hpp"




bool playoutground::setup(vkobjs& objs, std::string fname, int count) {
	if (!createubo(objs))return false;
	if (!loadmodel(objs, fname))return false;
	if (!createinstances(objs, count, false))return false;
	if (!createssbomat(objs))return false;


	updatemats();/////////////////////////////////////////////////

	return true;
}


bool playoutground::setup2(vkobjs& objs, std::string vfile, std::string ffile) {


	if (!createplayout(objs))return false;
	if (!createpline(objs, vfile, ffile))return false;
	return true;
}

bool playoutground::loadmodel(vkobjs& objs, std::string fname) {
	mgltf = std::make_shared<staticmodel>();
	if (!mgltf->loadmodel(objs, fname))return false;
	return true;
}

bool playoutground::createinstances(vkobjs& objs, int count, bool rand) {
	int numTriangles{};
	for (int i = 0; i < count; ++i) {
		minstances.emplace_back(std::make_shared<staticinstance>(mgltf, glm::vec3(0.0f), rand));
		numTriangles += mgltf->gettricount(0, 0);
	}
	totaltricount = numTriangles;
	numinstancess = count;

	if (!minstances.size())return false;
	return true;
}
bool playoutground::createubo(vkobjs& objs) {
	if (!ubo::init(objs, rdperspviewmatrixubo))return false;
	desclayouts.push_back(rdperspviewmatrixubo[0].rdubodescriptorlayout);
	return true;
}

bool playoutground::createssbomat(vkobjs& objs)
{
	size_t matssize = sizeof(glm::mat4);
	if (!ssbo::init(objs, rdmodelmatsssbo, matssize))return false;
	desclayouts.push_back(rdmodelmatsssbo.rdssbodescriptorlayout);
	return true;
}


bool playoutground::createplayout(vkobjs& objs) {
	vktexdatapls texdatapls0 = mgltf->gettexdatapls();
	desclayouts.insert(desclayouts.begin(), texdatapls0.texdescriptorlayout);
	if (!playout::init(objs, rdgltfpipelinelayout, desclayouts, sizeof(vkpushconstants)))return false;
	return true;
}

bool playoutground::createpline(vkobjs& objs, std::string vfile, std::string ffile) {
	//if (!gltfstaticpipeline::init(objs, rdgltfpipelinelayout, rdgltfgpupipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vfile, ffile))return false;
	if (!pline::init(objs, rdgltfpipelinelayout, rdgltfgpupipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 3, 7, std::vector<std::string>{vfile, ffile}))return false;
	return true;
}


void playoutground::uploadvboebo(vkobjs& objs, VkCommandBuffer& cbuffer) {
	if (uploadreq) {
		mgltf->uploadvboebo(objs,cbuffer);
		uploadreq = false;
	}
}

void playoutground::uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats) {
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1, &rdperspviewmatrixubo[0].rdubodescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1, &rdmodelmatsssbo.rdssbodescriptorset, 0, nullptr);

	ubo::upload(objs, rdperspviewmatrixubo, cammats, 0);
	ssbo::upload(objs, rdmodelmatsssbo, transmats);
}

std::shared_ptr<staticinstance> playoutground::getinst(int x)
{
	return minstances.at(x);
}


void playoutground::updatemats() {
	transmats.clear();
	staticsettings& settings = minstances[0]->getinstancesettings();
	if (settings.msdrawmodel) {
		glm::mat4 mat = minstances[0]->calcmat();
		transmats.push_back(mat);
	}
}

void playoutground::cleanuplines(vkobjs& objs) {
	pline::cleanup(objs, rdgltfgpupipeline);
	playout::cleanup(objs, rdgltfpipelinelayout);
}

void playoutground::cleanupbuffers(vkobjs& objs) {
	ubo::cleanup(objs, rdperspviewmatrixubo);
	ssbo::cleanup(objs, rdmodelmatsssbo);
}

void playoutground::cleanupmodels(vkobjs& objs) {
	mgltf->cleanup(objs);
	mgltf.reset();
}



void playoutground::draw(vkobjs& objs) {

	stride = 0;

	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1, &rdperspviewmatrixubo[0].rdubodescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1, &rdmodelmatsssbo.rdssbodescriptorset, 0, nullptr);

	vkCmdBindPipeline(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfgpupipeline);
	mgltf->drawinstanced(objs, rdgltfpipelinelayout, numinstancess, stride);

}
