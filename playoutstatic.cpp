#include "playoutstatic.hpp"




bool playoutstatic::setup(vkobjs& objs, std::string fname, int count) {
	if (!createubo(objs))return false;
	if (!loadmodel(objs, fname))return false;
	if (!createinstances(objs, count, false))return false;
	if (!createssbomat(objs))return false;


	updatemats();/////////////////////////////////////////////////

	return true;
}


bool playoutstatic::setup2(vkobjs& objs, std::string vfile, std::string ffile) {


	if (!createplayout(objs))return false;
	if (!createpline(objs, vfile, ffile))return false;
	return true;
}

bool playoutstatic::loadmodel(vkobjs& objs, std::string fname) {
	mgltf = std::make_shared<vkgltfstatic>();
	if (!mgltf->loadmodel(objs, fname))return false;
	return true;
}

bool playoutstatic::createinstances(vkobjs& objs, int count, bool rand) {
	int numTriangles{};
	for (int i = 0; i < count; ++i) {
		//int xPos = std::rand() % 9999;
		//int zPos = std::rand() % 9999;
		//minstances.emplace_back(std::make_shared<staticinstance>(mgltf, glm::vec2(static_cast<float>(xPos), static_cast<float>(zPos)), rand));
		if (i <1)
		minstances.emplace_back(std::make_shared<staticinstance>(mgltf, glm::vec3(0.0f,-100.0f, 0.0f ), rand));
		else {
			minstances.emplace_back(std::make_shared<staticinstance>(mgltf, glm::vec3(static_cast<float>((i%512)*50), 0.0f, static_cast<float>((i/512)*50)), rand));
		}
		numTriangles += mgltf->gettricount(0,0);
	}
	totaltricount = numTriangles;
	numinstancess = count;

	if (!minstances.size())return false;
	return true;
}
bool playoutstatic::createubo(vkobjs& objs) {
	if (!ubo::init(objs, rdperspviewmatrixubo))return false;
	desclayouts.push_back(rdperspviewmatrixubo[0].rdubodescriptorlayout);
	return true;
}

bool playoutstatic::createssbomat(vkobjs& objs)
{
	size_t matssize = numinstancess * sizeof(glm::mat4);
	if (!ssbo::init(objs, rdmodelmatsssbo, matssize))return false;
	desclayouts.push_back(rdmodelmatsssbo.rdssbodescriptorlayout);
	return true;
}


bool playoutstatic::createplayout(vkobjs& objs) {
	std::vector<vktexdata> texdata0 = mgltf->gettexdata();
	desclayouts.insert(desclayouts.begin(), texdata0[0].texdescriptorlayout);
	if (!playout::init(objs, rdgltfpipelinelayout, desclayouts, sizeof(vkpushconstants)))return false;
	return true;
}

bool playoutstatic::createpline(vkobjs& objs, std::string vfile, std::string ffile) {
	if (!gltfstaticpipeline::init(objs, rdgltfpipelinelayout, rdgltfgpupipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vfile, ffile))return false;
	return true;
}


void playoutstatic::uploadvboebo(vkobjs& objs) {
	if (uploadreq) {
		mgltf->uploadvertexbuffers(objs);
		mgltf->uploadindexbuffers(objs);
		uploadreq = false;
	}
}

void playoutstatic::uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats) {
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1, &rdperspviewmatrixubo[0].rdubodescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1, &rdmodelmatsssbo.rdssbodescriptorset, 0, nullptr);

	ubo::upload(objs, rdperspviewmatrixubo, cammats, 0);
	ssbo::upload(objs, rdmodelmatsssbo, transmats);
}

staticsettings playoutstatic::getinstsettings(int x){
	return minstances.at(x)->getinstancesettings();
}

std::shared_ptr<staticinstance> playoutstatic::getinst(int x)
{
	return minstances.at(x);
}


void playoutstatic::updatemats(){
	transmats.clear();
	for (auto& i : minstances) {
		staticsettings settings = i->getinstancesettings();
		if (!settings.msdrawmodel)continue;
		glm::mat4 mat = i->calcmat();
		transmats.push_back(mat);
	}
}

void playoutstatic::cleanuplines(vkobjs& objs) {
	gltfstaticpipeline::cleanup(objs, rdgltfgpupipeline);
	playout::cleanup(objs, rdgltfpipelinelayout);
}

void playoutstatic::cleanupbuffers(vkobjs& objs) {
	ubo::cleanup(objs, rdperspviewmatrixubo);
	ssbo::cleanup(objs, rdmodelmatsssbo);
}

void playoutstatic::cleanupmodels(vkobjs& objs) {
	mgltf->cleanup(objs);
	mgltf.reset();
}



void playoutstatic::draw(vkobjs& objs) {

	stride = 0;

	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1, &rdperspviewmatrixubo[0].rdubodescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1, &rdmodelmatsssbo.rdssbodescriptorset, 0, nullptr);

	vkCmdBindPipeline(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfgpupipeline);
	mgltf->drawinstanced(objs, rdgltfpipelinelayout, numinstancess, stride);

}
