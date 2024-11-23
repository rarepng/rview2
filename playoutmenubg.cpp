#include "playoutmenubg.hpp"




bool playoutmenubg::setup(vkobjs& objs, int count) {
	//if (!createubo(objs))return false;
	if (!loadmodel(objs))return false;
	//if (!createssbomat(objs))return false;


	//updatemats();/////////////////////////////////////////////////

	return true;
}


bool playoutmenubg::setup2(vkobjs& objs, std::string vfile, std::string ffile) {


	if (!createplayout(objs))return false;
	if (!createpline(objs, vfile, ffile))return false;
	return true;
}

bool playoutmenubg::loadmodel(vkobjs& objs) {
	if (!mmenu.loadmodel(objs))return false;
	return true;
}

//bool playoutmenubg::createubo(vkobjs& objs) {
//	if (!ubo::init(objs, rdperspviewmatrixubo))return false;
//	desclayouts.push_back(rdperspviewmatrixubo[0].rdubodescriptorlayout);
//	return true;
//}

//bool playoutmenubg::createssbomat(vkobjs& objs)
//{
//	size_t matssize = numinstancess * sizeof(glm::mat4);
//	if (!ssbo::init(objs, rdmodelmatsssbo, matssize))return false;
//	desclayouts.push_back(rdmodelmatsssbo.rdssbodescriptorlayout);
//	return true;
//}


bool playoutmenubg::createplayout(vkobjs& objs) {
	//std::vector<vktexdata> texdata0 = mgltf->gettexdata();
	//desclayouts.insert(desclayouts.begin(), texdata0[0].texdescriptorlayout);
	if (!playout::init(objs, rdgltfpipelinelayout, desclayouts, 3*sizeof(double)))return false;
	return true;
}

bool playoutmenubg::createpline(vkobjs& objs, std::string vfile, std::string ffile) {
	if (!menubgpipeline::init(objs, rdgltfpipelinelayout, rdgltfgpupipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vfile, ffile))return false;
	return true;
}


void playoutmenubg::uploadvboebo(vkobjs& objs) {
	if (uploadreq) {
		mmenu.uploadvertexbuffers(objs);
		mmenu.uploadindexbuffers(objs);
		uploadreq = false;
	}
}

//void playoutmenubg::uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats) {
	//vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1, &rdperspviewmatrixubo[0].rdubodescriptorset, 0, nullptr);
	//vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1, &rdmodelmatsssbo.rdssbodescriptorset, 0, nullptr);

	//ubo::upload(objs, rdperspviewmatrixubo, cammats, 0);
	//ssbo::upload(objs, rdmodelmatsssbo, transmats);
//}
void playoutmenubg::cleanuplines(vkobjs& objs) {
	menubgpipeline::cleanup(objs, rdgltfgpupipeline);
	playout::cleanup(objs, rdgltfpipelinelayout);
}

//void playoutmenubg::cleanupbuffers(vkobjs& objs) {
//	ubo::cleanup(objs, rdperspviewmatrixubo);
//	ssbo::cleanup(objs, rdmodelmatsssbo);
//}

void playoutmenubg::cleanupmodels(vkobjs& objs) {
	mmenu.cleanup(objs);
}



void playoutmenubg::draw(vkobjs& objs, double& time, double& time2, double& life) {


	//vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1, &rdperspviewmatrixubo[0].rdubodescriptorset, 0, nullptr);
	//vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1, &rdmodelmatsssbo.rdssbodescriptorset, 0, nullptr);

	vkCmdBindPipeline(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfgpupipeline);
	mmenu.draw(objs, rdgltfpipelinelayout, time, time2, life);

}
