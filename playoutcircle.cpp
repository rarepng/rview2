#include "playoutcircle.hpp"




bool playoutcircle::setup(vkobjs& objs, int count) {
	numinstancess = count;
	totaltricount = 2 * count;
	if (!createubo(objs))return false;
	if (!loadmodel(objs))return false;
	if (!createinstances(objs, count, false))return false;
	if (!createssbo(objs))return false;


	updatemats();/////////////////////////////////////////////////

	return true;
}


bool playoutcircle::setup2(vkobjs& objs, const std::string& tname, std::string vfile, std::string ffile) {
	if (!createplayout(objs,tname))return false;
	if (!createpline(objs, vfile, ffile))return false;
	return true;
}

bool playoutcircle::loadmodel(vkobjs& objs) {
	mquad = std::make_shared<texmodel>();
	if (!mquad->loadmodel(objs))return false;
	return true;
}

bool playoutcircle::createinstances(vkobjs& objs, int count, bool rand){
	minstances.clear();
	for (size_t i{ 0 }; i < count; i++) {
		minstances.emplace_back(std::make_shared<texinstance>(glm::vec3{ 0.0f }, rand));
	}
	//numinstancess = count;
	return true;
}

bool playoutcircle::createubo(vkobjs& objs) {
	if (!ubo::init(objs, mubo))return false;
	desclayouts.push_back(mubo[0].rdubodescriptorlayout);
	return true;
}

bool playoutcircle::createssbo(vkobjs& objs)
{
	if (!ssbo::init(objs, mssbo, numinstancess * sizeof(glm::mat4)))return false;
	desclayouts.push_back(mssbo.rdssbodescriptorlayout);
	if (!ssbo::init(objs, platessbo, numinstancess * sizeof(double)))return false;
	desclayouts.push_back(platessbo.rdssbodescriptorlayout);
	return true;
}

bool playoutcircle::createplayout(vkobjs& objs,const std::string& tname) {
	vktexture::loadtexturefile(objs,mtexture,mtexturepls, tname);
	desclayouts.insert(desclayouts.begin(), mtexturepls.texdescriptorlayout);
	if (!playout::init(objs, mplayout, desclayouts, 3 * sizeof(double)))return false;
	return true;
}

bool playoutcircle::createpline(vkobjs& objs, std::string vfile, std::string ffile) {
	if (!pline::init(objs, mplayout, mpline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,2,5,std::vector<std::string>{vfile,ffile}))return false;
	return true;
}


void playoutcircle::uploadvboebo(vkobjs& objs, VkCommandBuffer& cbuffer) {
	if (uploadreq) {
		mquad->uploadvboebo(objs,cbuffer);
		uploadreq = false;
	}
}

void playoutcircle::uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats,const std::vector<double>& enemylifes) {
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mplayout, 1, 1, &mubo[0].rdubodescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mplayout, 2, 1, &mssbo.rdssbodescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mplayout, 3, 1, &platessbo.rdssbodescriptorset, 0, nullptr);

	ubo::upload(objs, mubo, cammats, 0);
	ssbo::upload(objs, mssbo, transmats);
	ssbo::upload(objs, platessbo, enemylifes);
}

void playoutcircle::cleanuplines(vkobjs& objs) {
	pline::cleanup(objs, mpline);
	playout::cleanup(objs, mplayout);
}

void playoutcircle::cleanupbuffers(vkobjs& objs) {
	ubo::cleanup(objs, mubo);
	ssbo::cleanup(objs, mssbo);
	ssbo::cleanup(objs, platessbo);
}

void playoutcircle::cleanupmodels(vkobjs& objs) {
	mquad->cleanup(objs);
	vktexture::cleanup(objs, mtexture);
	vktexture::cleanuppls(objs, mtexturepls);
}


std::shared_ptr<texinstance> playoutcircle::getinst(int x){
	return minstances.at(x);
}

std::vector<std::shared_ptr<texinstance>>& playoutcircle::getallinstances(){
	return minstances;
}

void playoutcircle::updatemats() {
	transmats.clear();
	for (auto& i : minstances) {
		staticsettings& settings = i->getinstancesettings();
		if (!settings.msdrawmodel)continue;
		glm::mat4 mat = i->calcmat();
		transmats.push_back(mat);
	}
}



void playoutcircle::draw(vkobjs& objs, double& time, double& time2, double& life) {


	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mplayout, 0, 1, &mtexturepls.texdescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mplayout, 1, 1, &mubo[0].rdubodescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mplayout, 2, 1, &mssbo.rdssbodescriptorset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mplayout, 3, 1, &platessbo.rdssbodescriptorset, 0, nullptr);

	vkCmdBindPipeline(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mpline);
	mquad->draw(objs, mplayout,numinstancess, time, time2, life);

}
