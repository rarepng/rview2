#define GLM_ENABLE_EXPERIMENTAL

#include <chrono>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <cstdlib>
#include "vkgltfinstance.hpp"
vkgltfinstance::~vkgltfinstance(){}

vkgltfinstance::vkgltfinstance(std::shared_ptr<vkgltfmodel> model, glm::vec2 worldpos, bool randomize){

	if (!model)return;
	mgltfmodel = model;
	mmodelsettings.msworldpos = worldpos;
	mnodecount = mgltfmodel->getnodecount();
	minversebindmats = mgltfmodel->getinversebindmats();
	mnodetojoint = mgltfmodel->getnodetojoint();
	mjointmats.reserve(minversebindmats.size());
	mjointmats.resize(minversebindmats.size());
	mjointdualquats.reserve(minversebindmats.size());
	mjointdualquats.resize(minversebindmats.size());
	madditiveanimationmask.reserve(mnodecount);
	madditiveanimationmask.resize(mnodecount);
	minvertedadditiveanimationmask.reserve(mnodecount);
	minvertedadditiveanimationmask.resize(mnodecount);
	std::fill(madditiveanimationmask.begin(), madditiveanimationmask.end(), true);
	minvertedadditiveanimationmask = madditiveanimationmask;
	minvertedadditiveanimationmask.flip();

	gltfnodedata nodedata;
	nodedata = mgltfmodel->getgltfnodes();
	mrootnode = nodedata.rootnode;
	mrootnode->setwpos(glm::vec3(mmodelsettings.msworldpos.x, 0.0f, mmodelsettings.msworldpos.y));

	mnodelist = nodedata.nodelist;

	mmodelsettings.msskelsplitnode = mnodecount - 1;
	for (const auto& node : mnodelist) {
		if (node) {
			mmodelsettings.msskelnodenames.push_back(node->getname());
		}
		else {
			mmodelsettings.msskelnodenames.push_back("(invalid)");
		}
	}
	updatenodematrices(mrootnode);




	manimclips = mgltfmodel->getanimclips();



	for (const auto& clip : manimclips) {
		mmodelsettings.msclipnames.push_back(clip->getName());
	}
	unsigned int animclipsize = manimclips.size();

	if (randomize) {
		int animclip = std::rand() % animclipsize;
		float animclipspeed = (std::rand() % 100) / 100.0f + 0.5f;
		float initrotation = std::rand() % 360 - 180;

		mmodelsettings.msanimclip = animclip;
		mmodelsettings.msanimspeed = animclipspeed;
		mmodelsettings.msworldrot = glm::vec3(0.0f,initrotation,0.0f);
		mrootnode->setwrot(mmodelsettings.msworldrot);

	}

	checkforupdates();

	mskeletonmesh = std::make_shared<vkmesh>();
	mskeletonmesh->verts.reserve(mnodecount * 2);
	mskeletonmesh->verts.resize(mnodecount * 2);

	//hardcoded care
	mmodelsettings.msikeffectornode = 19;
	mmodelsettings.msikrootnode = 26;
	setinversekindematicsnode(mmodelsettings.msikeffectornode, mmodelsettings.msikrootnode);
	setnumikiterations(mmodelsettings.msikiterations);

	mmodelsettings.msiktargetworldpos = getwrot() * mmodelsettings.msiktargetpos + glm::vec3(worldpos.x, 0.0f, worldpos.y);


}

void vkgltfinstance::resetnodedata() {
	mgltfmodel->resetnodedata(mrootnode);
	updatenodematrices(mrootnode);
}

std::shared_ptr<vkmesh> vkgltfinstance::getskeleton(){
	mskeletonmesh->verts.clear();
	getskeletonpernode(mrootnode->getchildren().at(0));
	return mskeletonmesh;
}

void vkgltfinstance::getskeletonpernode(std::shared_ptr<vknode> treenode){

	glm::vec3 parentpos{ 0.0f };
	parentpos = glm::vec3(treenode->getnodematrix() * glm::vec4(1.0f));
	vkvert parentvert;
	parentvert.pos = parentpos;
	parentvert.col = glm::vec3(0.0f, 1.0f, 1.0f);

	for (const auto& child : treenode->getchildren()) {
		glm::vec3 childpos{ 0.0f };
		childpos = glm::vec3(child->getnodematrix() * glm::vec4(1.0f));

		vkvert childvert;
		childvert.pos = childpos;
		childvert.col = glm::vec3(0.0f, 0.0f, 1.0f);
		mskeletonmesh->verts.emplace_back(parentvert);
		mskeletonmesh->verts.emplace_back(childvert);

		getskeletonpernode(child);
	}

}


void vkgltfinstance::updatenodematrices(std::shared_ptr<vknode>treenode) {
	treenode->calculatenodemat();
	if (mmodelsettings.mvertexskinningmode == skinningmode::linear) {
		updatejointmatrices(treenode);
	}
	else {
		updatejointdualquats(treenode);
	}
	for (auto& child : treenode->getchildren()) {
		updatenodematrices(child);
	}
}


void vkgltfinstance::updatejointmatrices(std::shared_ptr<vknode>treenode) {
	int nodenum = treenode->getnum();
	mjointmats.at(mnodetojoint.at(nodenum)) = treenode->getnodematrix() * minversebindmats.at(mnodetojoint.at(nodenum));
}


void vkgltfinstance::updatejointdualquats(std::shared_ptr<vknode>treenode) {

	int nodenum = treenode->getnum();
	glm::quat orientation;
	glm::vec3 scale0;
	glm::vec3 trans0;
	glm::vec3 skew0;
	glm::vec4 pers0;
	glm::dualquat dq0;

	glm::mat4 nodejointmat = treenode->getnodematrix() * minversebindmats.at(mnodetojoint.at(nodenum));
	if (glm::decompose(nodejointmat, scale0, orientation, trans0, skew0, pers0)) {
		dq0[0] = orientation;
		dq0[1] = glm::quat(0.0f, trans0.x, trans0.y, trans0.z) * orientation * 0.5f;
		mjointdualquats.at(mnodetojoint.at(nodenum)) = glm::mat2x4_cast(dq0);
	}

}
int vkgltfinstance::getjointmatrixsize() {
	return mjointmats.size();
}

std::vector<glm::mat4> vkgltfinstance::getjointmats() {
	return mjointmats;
}
int vkgltfinstance::getjointdualquatssize() {
	return mjointdualquats.size();
}
std::vector<glm::mat2x4> vkgltfinstance::getjointdualquats() {
	return mjointdualquats;
}



void vkgltfinstance::checkforupdates() {
	static blendmode lastBlendMode = mmodelsettings.msblendingmode;
	static int skelSplitNode = mmodelsettings.msskelsplitnode;
	static glm::vec2 worldPos = mmodelsettings.msworldpos;
	static glm::vec3 worldRot = mmodelsettings.msworldrot;
	static glm::vec3 ikTargetPos = mmodelsettings.msiktargetpos;
	static ikmode lastIkMode = mmodelsettings.msikmode;
	static int numIKIterations = mmodelsettings.msikiterations;
	static int ikEffectorNode = mmodelsettings.msikeffectornode;
	static int ikRootNode = mmodelsettings.msikrootnode;

	if (skelSplitNode != mmodelsettings.msskelsplitnode) {
		setskeletonsplitnode(mmodelsettings.msskelsplitnode);
		skelSplitNode = mmodelsettings.msskelsplitnode;
		resetnodedata();
	}

	if (lastBlendMode != mmodelsettings.msblendingmode) {
		lastBlendMode = mmodelsettings.msblendingmode;
		if (mmodelsettings.msblendingmode != blendmode::additive) {
			mmodelsettings.msskelsplitnode = mnodecount - 1;
		}
		resetnodedata();
	}

	if (worldPos != mmodelsettings.msworldpos) {
		mrootnode->setwpos(glm::vec3(mmodelsettings.msworldpos.x, 0.0f,
			mmodelsettings.msworldpos.y));
		worldPos = mmodelsettings.msworldpos;
		mmodelsettings.msiktargetworldpos = getwrot() *
			mmodelsettings.msiktargetpos + glm::vec3(worldPos.x, 0.0f, worldPos.y);
	}

	if (worldRot != mmodelsettings.msworldrot) {
		mrootnode->setwrot(mmodelsettings.msworldrot);
		worldRot = mmodelsettings.msworldrot;
		mmodelsettings.msiktargetworldpos = getwrot() *
			mmodelsettings.msiktargetpos + glm::vec3(worldPos.x, 0.0f, worldPos.y);
	}

	if (ikTargetPos != mmodelsettings.msiktargetpos) {
		ikTargetPos = mmodelsettings.msiktargetpos;
		mmodelsettings.msiktargetworldpos = getwrot() *
			mmodelsettings.msiktargetpos + glm::vec3(worldPos.x, 0.0f, worldPos.y);
	}

	if (lastIkMode != mmodelsettings.msikmode) {
		resetnodedata();
		lastIkMode = mmodelsettings.msikmode;
	}

	if (numIKIterations != mmodelsettings.msikiterations) {
		setnumikiterations(mmodelsettings.msikiterations);
		resetnodedata();
		numIKIterations = mmodelsettings.msikiterations;
	}

	if (ikEffectorNode != mmodelsettings.msikeffectornode ||
		ikRootNode != mmodelsettings.msikrootnode) {
		setinversekindematicsnode(mmodelsettings.msikeffectornode, mmodelsettings.msikrootnode);
		resetnodedata();
		ikEffectorNode = mmodelsettings.msikeffectornode;
		ikRootNode = mmodelsettings.msikrootnode;
	}
}


void vkgltfinstance::updateanimation() {
	if (mmodelsettings.msplayanimation) {
		if (mmodelsettings.msblendingmode == blendmode::crossfade ||
			mmodelsettings.msblendingmode == blendmode::additive) {
			playanimation(mmodelsettings.msanimclip,
				mmodelsettings.mscrossblenddestanimclip, mmodelsettings.msanimspeed,
				mmodelsettings.msanimcrossblendfactor,
				mmodelsettings.msanimationplaydirection);
		}
		else {
			playanimation(mmodelsettings.msanimclip, mmodelsettings.msanimspeed,
				mmodelsettings.msanimblendfactor,
				mmodelsettings.msanimationplaydirection);
		}
	}
	else {
		mmodelsettings.msanimendtime = getanimendtime(mmodelsettings.msanimclip);
		if (mmodelsettings.msblendingmode == blendmode::crossfade ||
			mmodelsettings.msblendingmode == blendmode::additive) {
			crossblendanimationframe(mmodelsettings.msanimclip,
				mmodelsettings.mscrossblenddestanimclip, mmodelsettings.msanimtimepos,
				mmodelsettings.msanimcrossblendfactor);
		}
		else {
			blendanimationframe(mmodelsettings.msanimclip, mmodelsettings.msanimtimepos,
				mmodelsettings.msanimblendfactor);
		}
	}
}

void vkgltfinstance::solveik() {
	switch (mmodelsettings.msikmode) {
	case ikmode::ccd:
		solveikbyccd(mmodelsettings.msiktargetworldpos);
		break;
	case ikmode::fabrik:
		solveikbyfabrik(mmodelsettings.msiktargetworldpos);
		break;
	default:
		/* do nothing */
		break;
	}
}





void vkgltfinstance::playanimation(int animNum, float speedDivider, float blendFactor,
	replaydirection direction) {
	double currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	if (direction == replaydirection::backward) {
		blendanimationframe(animNum, manimclips.at(animNum)->getEndTime() -
			std::fmod(currentTime / 1000.0 * speedDivider,
				manimclips.at(animNum)->getEndTime()), blendFactor);
	}
	else {
		blendanimationframe(animNum, std::fmod(currentTime / 1000.0 * speedDivider,
			manimclips.at(animNum)->getEndTime()), blendFactor);
	}
}

void vkgltfinstance::playanimation(int sourceAnimNumber, int destAnimNumber,
	float speedDivider, float blendFactor, replaydirection direction) {
	double currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

	if (direction == replaydirection::backward) {
		crossblendanimationframe(sourceAnimNumber, destAnimNumber,
			manimclips.at(sourceAnimNumber)->getEndTime() -
			std::fmod(currentTime / 1000.0 * speedDivider,
				manimclips.at(sourceAnimNumber)->getEndTime()), blendFactor);
	}
	else {
		crossblendanimationframe(sourceAnimNumber, destAnimNumber,
			std::fmod(currentTime / 1000.0 * speedDivider,
				manimclips.at(sourceAnimNumber)->getEndTime()), blendFactor);
	}
}

void vkgltfinstance::blendanimationframe(int animNum, float time, float blendFactor) {
	manimclips.at(animNum)->blendFrame(mnodelist, madditiveanimationmask, time,
		blendFactor);
	updatenodematrices(mrootnode);
}

void vkgltfinstance::crossblendanimationframe(int sourceAnimNumber, int destAnimNumber,
	float time, float blendFactor) {

	float sourceAnimDuration = manimclips.at(sourceAnimNumber)->getEndTime();
	float destAnimDuration = manimclips.at(destAnimNumber)->getEndTime();

	float scaledTime = time * (destAnimDuration / sourceAnimDuration);

	manimclips.at(sourceAnimNumber)->setFrame(mnodelist, madditiveanimationmask, time);
	manimclips.at(destAnimNumber)->blendFrame(mnodelist, madditiveanimationmask,
		scaledTime, blendFactor);

	manimclips.at(destAnimNumber)->setFrame(mnodelist, minvertedadditiveanimationmask,
		scaledTime);
	manimclips.at(sourceAnimNumber)->blendFrame(mnodelist,
		minvertedadditiveanimationmask, time, blendFactor);

	updatenodematrices(mrootnode);
}

void vkgltfinstance::updateadditivemask(std::shared_ptr<vknode> treeNode, int splitNodeNum) {
	/* break chain here */
	if (treeNode->getnum() == splitNodeNum) {
		return;
	}

	madditiveanimationmask.at(treeNode->getnum()) = false;
	for (auto& childNode : treeNode->getchildren()) {
		updateadditivemask(childNode, splitNodeNum);
	}
}

void vkgltfinstance::setskeletonsplitnode(int nodeNum) {
	std::fill(madditiveanimationmask.begin(), madditiveanimationmask.end(), true);
	updateadditivemask(mrootnode, nodeNum);

	minvertedadditiveanimationmask = madditiveanimationmask;
	minvertedadditiveanimationmask.flip();
}

void vkgltfinstance::setinstancesettings(modelsettings settings) {
	mmodelsettings = settings;
}

modelsettings vkgltfinstance::getinstancesettings() {
	return mmodelsettings;
}

glm::vec2 vkgltfinstance::getwpos() {
	return mmodelsettings.msworldpos;
}

glm::quat vkgltfinstance::getwrot() {
	return glm::normalize(glm::quat(glm::vec3(
		glm::radians(mmodelsettings.msworldrot.x),
		glm::radians(mmodelsettings.msworldrot.y),
		glm::radians(mmodelsettings.msworldrot.z)
	)));
}

float vkgltfinstance::getanimendtime(int animNum) {
	return manimclips.at(animNum)->getEndTime();
}

void vkgltfinstance::setinversekindematicsnode(int effectorNodeNum, int ikChainRootNodeNum) {
	if (effectorNodeNum < 0 || effectorNodeNum >(mnodelist.size() - 1)) {
		return;
	}

	if (ikChainRootNodeNum < 0 || ikChainRootNodeNum >(mnodelist.size() - 1)) {
		return;
	}

	std::vector<std::shared_ptr<vknode>> ikNodes{};
	int currentNodeNum = effectorNodeNum;

	ikNodes.insert(ikNodes.begin(), mnodelist.at(effectorNodeNum));
	while (currentNodeNum != ikChainRootNodeNum) {
		std::shared_ptr<vknode> node = mnodelist.at(currentNodeNum);
		if (node) {
			std::shared_ptr<vknode> parentNode = node->getparent();
			if (parentNode) {
				currentNodeNum = parentNode->getnum();
				ikNodes.push_back(parentNode);
			}
			else {
				/* force stopping on the root node */
				break;
			}
		}
	}

	miksolver.setnodes(ikNodes);
}

void vkgltfinstance::setnumikiterations(int iterations) {
	miksolver.setnumiterations(iterations);
}

void vkgltfinstance::solveikbyccd(glm::vec3 target) {
	miksolver.solveccd(target);
	updatenodematrices(miksolver.getikchainrootnode());
}

void vkgltfinstance::solveikbyfabrik(glm::vec3 target) {
	miksolver.solvefabrik(target);
	updatenodematrices(miksolver.getikchainrootnode());
}
