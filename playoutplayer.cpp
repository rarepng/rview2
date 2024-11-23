#include "playoutplayer.hpp"

bool playoutplayer::setup(vkobjs& objs, std::string fname, int count) {
	if (!createubo(objs))return false;
	if (!loadmodel(objs, fname))return false;
	if (!createinstances(objs, count, false))return false;
	if (!createssbomat(objs))return false;
	if (!createssbodq(objs))return false;
	if (!createssbodecay(objs))return false;

	return true;
}


bool playoutplayer::setup2(vkobjs& objs, std::string vfile, std::string ffile) {


	if (!createplayout(objs))return false;
	if (!createpline(objs, vfile, ffile))return false;
	//if (!createpline2(objs, "shaders/gltf_gpu_dquat.vert.spv", "shaders/gltf_gpu_dquat.frag.spv"))return false;
	//if (!createdecaypline(objs, "shaders/decay.vert.spv", "shaders/decay.frag.spv"))return false;
	if (!createpline2(objs, "shaders/gltf_gpu_dquat.vert.spv", "shaders/gltf_gpu_dquat.frag.spv"))return false;
	if (!createdecaypline(objs, "shaders/decay.vert.spv", "shaders/decay.frag.spv"))return false;
	return true;
}

bool playoutplayer::loadmodel(vkobjs& objs, std::string fname) {
	mgltf = std::make_shared<vkgltfmodel>();
	if (!mgltf->loadmodel(objs, fname))return false;
	return true;
}

bool playoutplayer::createinstances(vkobjs& objs, int count, bool rand) {
	int numTriangles{};
	for (int i = 0; i < count; ++i) {
		minstances.emplace_back(std::make_shared<vkgltfinstance>(mgltf, glm::vec3{ 0.0f, 0.0f, 0.0f }, rand));
		numTriangles += mgltf->gettricount(0, 0);
	}
	totaltricount = numTriangles;
	numinstancess = count;

	if (!minstances.size())return false;
	return true;
}
bool playoutplayer::createdecayinstances(vkobjs& objs){
	decayinstances.clear();
	for (int i{ 0 }; i < 20; i++) {
		float x = minstances[0]->getinstancesettings().msworldpos.x;
		float y = minstances[0]->getinstancesettings().msworldpos.y;
		decayinstances.emplace_back(std::make_shared<vkgltfinstance>(mgltf, glm::vec3{ x, 0.0f, y }, false));
	}
	return true;
}
bool playoutplayer::createubo(vkobjs& objs) {
	if (!ubo::init(objs, rdperspviewmatrixubo))return false;
	desclayouts.push_back(rdperspviewmatrixubo[0].rdubodescriptorlayout);
	return true;
}


bool playoutplayer::createssbomat(vkobjs& objs) {
	size_t size = numinstancess * minstances[0]->getjointmatrixsize() * sizeof(glm::mat4);
	if (!ssbo::init(objs, rdjointmatrixssbo, size))return false;
	desclayouts.push_back(rdjointmatrixssbo.rdssbodescriptorlayout);
	return true;
}

bool playoutplayer::createssbodq(vkobjs& objs) {
	size_t size = (numinstancess)*minstances[0]->getjointdualquatssize() * sizeof(glm::mat2x4);
	if (!ssbo::init(objs, rdjointdualquatssbo, size))return false;
	desclayouts.push_back(rdjointdualquatssbo.rdssbodescriptorlayout);
	return true;
}
bool playoutplayer::createssbodecay(vkobjs& objs) {
	size_t size = minstances[0]->getjointmatrixsize() * sizeof(glm::mat4);
	if (!ssbo::init(objs, rdjointdecay, size))return false;
	desclayouts.push_back(rdjointdecay.rdssbodescriptorlayout);
	return true;
}

bool playoutplayer::createplayout(vkobjs& objs) {
	std::vector<vktexdata> texdata0 = mgltf->gettexdata();
	desclayouts.insert(desclayouts.begin(), texdata0[0].texdescriptorlayout);
	if (!playout::init(objs, rdgltfpipelinelayout, desclayouts, sizeof(vkpushconstants)))return false;
	return true;
}

bool playoutplayer::createpline(vkobjs& objs, std::string vfile, std::string ffile) {
	if (!gltfgpupipeline::init(objs, rdgltfpipelinelayout, rdgltfgpupipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vfile, ffile))return false;
	return true;
}

bool playoutplayer::createpline2(vkobjs& objs, std::string vfile, std::string ffile) {
	if (!gltfgpupipeline::init(objs, rdgltfpipelinelayout, rdgltfgpudqpipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vfile, ffile))return false;
	return true;
}

bool playoutplayer::createdecaypline(vkobjs& objs, std::string vfile, std::string ffile){
	if (!gltfgpupipeline::init(objs, rdgltfpipelinelayout, decaypline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vfile, ffile))return false;
	return true;
}


void playoutplayer::updateanims() {
	for (auto& i : minstances) {
		i->updateanimation();
		i->solveik();
	}

}

void playoutplayer::uploadvboebo(vkobjs& objs) {
	if (uploadreq) {
		mgltf->uploadvertexbuffers(objs);
		mgltf->uploadindexbuffers(objs);
		uploadreq = false;
	}
}

void playoutplayer::uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats) {
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1, &rdperspviewmatrixubo[0].rdubodescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1, &rdjointmatrixssbo.rdssbodescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 3, 1, &rdjointdualquatssbo.rdssbodescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 4, 1, &rdjointdecay.rdssbodescriptorset, 0, nullptr);

	ubo::upload(objs, rdperspviewmatrixubo, cammats, 0);
	ssbo::upload(objs, rdjointmatrixssbo, jointmats);
	ssbo::upload(objs, rdjointdualquatssbo, jointdqs);
	ssbo::upload(objs, rdjointdecay, decaymat);

}

std::shared_ptr<vkgltfinstance> playoutplayer::getinst(int i) {
	return minstances[i];
}

modelsettings playoutplayer::getinstsettings()
{
	return minstances.at(0)->getinstancesettings();
}

void playoutplayer::updatemats() {

	totaltricount = 0;
	jointmats.clear();
	jointdqs.clear();

	numdqs = 0;
	nummats = 0;

	for (const auto& i : minstances) {
		modelsettings settings = i->getinstancesettings();
		if (!settings.msdrawmodel)continue;
		if (settings.mvertexskinningmode == skinningmode::dualquat) {
			std::vector<glm::mat2x4> quats = i->getjointdualquats();
			jointdqs.insert(jointdqs.end(), quats.begin(), quats.end());
			numdqs++;
		}
		else {
			std::vector<glm::mat4> mats = i->getjointmats();
			jointmats.insert(jointmats.end(), mats.begin(), mats.end());
			nummats++;
		}
		totaltricount += mgltf->gettricount(0, 0);
	}
}

void playoutplayer::freezedecay(){
	decaymat.clear();
	minstances[0]->checkforupdates();
	minstances[0]->updateanimation();
	std::vector<glm::mat4> mats = minstances[0]->getjointmats();
	decaymat.insert(decaymat.end(), mats.begin(), mats.end());
}

void playoutplayer::cleanuplines(vkobjs& objs) {
	gltfgpupipeline::cleanup(objs, rdgltfgpudqpipeline);
	gltfgpupipeline::cleanup(objs, rdgltfgpupipeline);
	gltfgpupipeline::cleanup(objs, decaypline);
	playout::cleanup(objs, rdgltfpipelinelayout);
}

void playoutplayer::cleanupbuffers(vkobjs& objs) {
	ubo::cleanup(objs, rdperspviewmatrixubo);
	ssbo::cleanup(objs, rdjointdualquatssbo);
	ssbo::cleanup(objs, rdjointmatrixssbo);
	ssbo::cleanup(objs, rdjointdecay);
}

void playoutplayer::cleanupmodels(vkobjs& objs) {
	mgltf->cleanup(objs);
	mgltf.reset();
}



void playoutplayer::draw(vkobjs& objs) {
	if (minstances[0]->getinstancesettings().msdrawmodel) {
		stride = minstances.at(0)->getjointmatrixsize();
		stridedq = minstances.at(0)->getjointdualquatssize();

		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1, &rdperspviewmatrixubo[0].rdubodescriptorset, 0, nullptr);
		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1, &rdjointmatrixssbo.rdssbodescriptorset, 0, nullptr);
		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 3, 1, &rdjointdualquatssbo.rdssbodescriptorset, 0, nullptr);
		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 4, 1, &rdjointdecay.rdssbodescriptorset, 0, nullptr);

		vkCmdBindPipeline(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfgpupipeline);
		mgltf->drawinstanced(objs, rdgltfpipelinelayout, numinstancess, stride);
		vkCmdBindPipeline(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfgpudqpipeline);
		mgltf->drawinstanced(objs, rdgltfpipelinelayout, numinstancess, stridedq);
	}

}

void playoutplayer::drawdecays(vkobjs& objs, double& decaytime, bool* decaying){

	if (minstances[0]->getinstancesettings().msdrawmodel) {
		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1, &rdperspviewmatrixubo[0].rdubodescriptorset, 0, nullptr);
		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1, &rdjointmatrixssbo.rdssbodescriptorset, 0, nullptr);
		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 3, 1, &rdjointdualquatssbo.rdssbodescriptorset, 0, nullptr);
		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 4, 1, &rdjointdecay.rdssbodescriptorset, 0, nullptr);

		vkCmdBindPipeline(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, decaypline);
		mgltf->drawinstanced(objs, rdgltfpipelinelayout, decayinstances.size(), stride, decaytime, decaying);
	}
}
