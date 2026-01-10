#define GLM_ENABLE_EXPERIMENTAL

#include "genericinstance.hpp"
#include <chrono>
#include <cstdlib>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/string_cast.hpp>
genericinstance::~genericinstance() {}

genericinstance::genericinstance(std::shared_ptr<genericmodel> model, glm::vec3 worldpos, bool randomize) {
	if (!model)
		return;
	mgltfmodel = model;
	mmodelsettings.msworldpos = worldpos;
	mnodecount = mgltfmodel->getnodecount();
	if (mnodecount) {
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
		mrootnode->setwpos(
		    glm::vec3(mmodelsettings.msworldpos.x, mmodelsettings.msworldpos.y, mmodelsettings.msworldpos.z));

		mnodelist = nodedata.nodelist;

		mmodelsettings.msskelsplitnode = mnodecount - 1;
		for (const auto &node : mnodelist) {
			if (node) {
				mmodelsettings.msskelnodenames.push_back(node->getname());
			} else {
				mmodelsettings.msskelnodenames.push_back("(invalid)");
			}
		}
		updatenodematrices(mrootnode);

		manimclips = mgltfmodel->getanimclips();

		for (const auto &clip : manimclips) {
			mmodelsettings.msclipnames.push_back(clip->getName());
		}
		unsigned int animclipsize = manimclips.size();

		if (randomize) {
			int animclip = std::rand() % animclipsize;
			float animclipspeed = (std::rand() % 100) / 100.0f + 0.5f;
			float initrotation = std::rand() % 360 - 180;

			mmodelsettings.msanimclip = animclip;
			mmodelsettings.msanimspeed = animclipspeed;
			mmodelsettings.msworldrot = glm::vec3(0.0f, initrotation, 0.0f);
			mrootnode->setwrot(mmodelsettings.msworldrot);
		}
	}
	checkforupdates();

	if (mnodecount) {

		// hardcoded
		mmodelsettings.msikeffectornode = 19;
		mmodelsettings.msikrootnode = 26;
		setinversekindematicsnode(mmodelsettings.msikeffectornode, mmodelsettings.msikrootnode);
		setnumikiterations(mmodelsettings.msikiterations);

		mmodelsettings.msiktargetworldpos =
		    getwrot() * mmodelsettings.msiktargetpos + glm::vec3(worldpos.x, 0.0f, worldpos.y);
	}
}

void genericinstance::resetnodedata() {
	mgltfmodel->resetnodedata(mrootnode);
	updatenodematrices(mrootnode);
}

void genericinstance::updatenodematrices(std::shared_ptr<vknode> treenode) {
	treenode->calculatenodemat();
	if (mmodelsettings.mvertexskinningmode == skinningmode::linear) {
		updatejointmatrices(treenode);
	} else {
		updatejointdualquats(treenode);
	}
	for (auto &child : treenode->getchildren()) {
		updatenodematrices(child);
	}
}

void genericinstance::updatejointmatrices(std::shared_ptr<vknode> treenode) {
	int nodenum = treenode->getnum();
	mjointmats.at(mnodetojoint.at(nodenum)) = treenode->getnodematrix() * minversebindmats.at(mnodetojoint.at(nodenum));
}

void genericinstance::updatejointdualquats(std::shared_ptr<vknode> treenode) {

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
int genericinstance::getjointmatrixsize() {
	return mjointmats.size();
}

std::vector<glm::mat4> genericinstance::getjointmats() {
	return mjointmats;
}
int genericinstance::getjointdualquatssize() {
	return mjointdualquats.size();
}
std::vector<glm::mat2x4> genericinstance::getjointdualquats() {
	return mjointdualquats;
}

void genericinstance::checkforupdates() {

std::memset(&mLastState, 0, sizeof(mLastState));
	
    if (!mnodecount) [[unlikely]] return;

    auto& curr = mmodelsettings;
    auto reactions = std::tuple {
        
        Reaction(&InstanceState::skelSplitNode, [](auto* self, int newVal) {
            self->setskeletonsplitnode(newVal);
            self->resetnodedata();
        }),

        Reaction(&InstanceState::blendingMode, [](auto* self, blendmode newVal) {
            if (newVal != blendmode::additive) {
                self->mmodelsettings.msskelsplitnode = self->mnodecount - 1;
            }
            self->resetnodedata();
        }),

        Reaction(&InstanceState::worldScale, [](auto* self, const glm::vec3& newScale) {
            self->mrootnode->setscale(newScale);
            self->mmodelsettings.msiktargetworldpos = 
                self->getwrot() * self->mmodelsettings.msiktargetpos + self->mmodelsettings.msworldpos;
        }),

        Reaction(&InstanceState::worldPos, [](auto* self, const glm::vec3& newPos) {
            self->mrootnode->setwpos(newPos);
            self->mmodelsettings.msiktargetworldpos = 
                self->getwrot() * self->mmodelsettings.msiktargetpos + newPos;
        }),
        
        Reaction(&InstanceState::ikIterations, [](auto* self, int newIter) {
             self->setnumikiterations(newIter);
             self->resetnodedata();
        })
        
    };

    std::apply([&](auto&&... args) {
        process_changes(this,
            reinterpret_cast<InstanceState&>(curr),
            mLastState,
            args...
        );
    }, reactions);
}

// void genericinstance::checkforupdates() {
// 	static glm::vec3 worldPos = mmodelsettings.msworldpos;
// 	static glm::vec3 worldRot = mmodelsettings.msworldrot;
// 	static glm::vec3 worldScale = mmodelsettings.msworldscale;

// 	if (mnodecount) {
// 		static blendmode lastBlendMode = mmodelsettings.msblendingmode;
// 		static int skelSplitNode = mmodelsettings.msskelsplitnode;
// 		static glm::vec3 ikTargetPos = mmodelsettings.msiktargetpos;
// 		static ikmode lastIkMode = mmodelsettings.msikmode;
// 		static int numIKIterations = mmodelsettings.msikiterations;
// 		static int ikEffectorNode = mmodelsettings.msikeffectornode;
// 		static int ikRootNode = mmodelsettings.msikrootnode;

// 		if (skelSplitNode != mmodelsettings.msskelsplitnode) {
// 			setskeletonsplitnode(mmodelsettings.msskelsplitnode);
// 			skelSplitNode = mmodelsettings.msskelsplitnode;
// 			resetnodedata();
// 		}

// 		if (lastBlendMode != mmodelsettings.msblendingmode) {
// 			lastBlendMode = mmodelsettings.msblendingmode;
// 			if (mmodelsettings.msblendingmode != blendmode::additive) {
// 				mmodelsettings.msskelsplitnode = mnodecount - 1;
// 			}
// 			resetnodedata();
// 		}
// 		if (worldScale != mmodelsettings.msworldscale) {
// 			mrootnode->setscale(mmodelsettings.msworldscale);
// 			worldScale = mmodelsettings.msworldscale;
// 			mmodelsettings.msiktargetworldpos = getwrot() * mmodelsettings.msiktargetpos + worldPos;
// 		}

// 		if (worldPos != mmodelsettings.msworldpos) {
// 			mrootnode->setwpos(mmodelsettings.msworldpos);
// 			worldPos = mmodelsettings.msworldpos;
// 			mmodelsettings.msiktargetworldpos = getwrot() * mmodelsettings.msiktargetpos + worldPos;
// 		}

// 		if (worldRot != mmodelsettings.msworldrot) {
// 			mrootnode->setwrot(mmodelsettings.msworldrot);
// 			worldRot = mmodelsettings.msworldrot;
// 			mmodelsettings.msiktargetworldpos = getwrot() * mmodelsettings.msiktargetpos + worldPos;
// 		}

// 		if (ikTargetPos != mmodelsettings.msiktargetpos) {
// 			ikTargetPos = mmodelsettings.msiktargetpos;
// 			mmodelsettings.msiktargetworldpos = getwrot() * mmodelsettings.msiktargetpos + worldPos;
// 		}

// 		if (lastIkMode != mmodelsettings.msikmode) {
// 			resetnodedata();
// 			lastIkMode = mmodelsettings.msikmode;
// 		}

// 		if (numIKIterations != mmodelsettings.msikiterations) {
// 			setnumikiterations(mmodelsettings.msikiterations);
// 			resetnodedata();
// 			numIKIterations = mmodelsettings.msikiterations;
// 		}

// 		if (ikEffectorNode != mmodelsettings.msikeffectornode || ikRootNode != mmodelsettings.msikrootnode) {
// 			setinversekindematicsnode(mmodelsettings.msikeffectornode, mmodelsettings.msikrootnode);
// 			resetnodedata();
// 			ikEffectorNode = mmodelsettings.msikeffectornode;
// 			ikRootNode = mmodelsettings.msikrootnode;
// 		}
// 	}
// }

void genericinstance::updateanimation() {
	if (mmodelsettings.msplayanimation) {
		mmodelsettings.msanimendtime = getanimendtime(mmodelsettings.msanimclip);
		if (mmodelsettings.msblendingmode == blendmode::crossfade || mmodelsettings.msblendingmode == blendmode::additive) {
			playanimation(mmodelsettings.msanimclip, mmodelsettings.mscrossblenddestanimclip, mmodelsettings.msanimspeed,
			              mmodelsettings.msanimcrossblendfactor, mmodelsettings.msanimationplaydirection);
		} else {
			playanimation(mmodelsettings.msanimclip, mmodelsettings.msanimspeed, mmodelsettings.msanimblendfactor,
			              mmodelsettings.msanimationplaydirection);
		}
	} else {
		mmodelsettings.msanimendtime = getanimendtime(mmodelsettings.msanimclip);
		if (mmodelsettings.msblendingmode == blendmode::crossfade || mmodelsettings.msblendingmode == blendmode::additive) {
			crossblendanimationframe(mmodelsettings.msanimclip, mmodelsettings.mscrossblenddestanimclip,
			                         mmodelsettings.msanimtimepos, mmodelsettings.msanimcrossblendfactor);
		} else {
			blendanimationframe(mmodelsettings.msanimclip, mmodelsettings.msanimtimepos, mmodelsettings.msanimblendfactor);
		}
	}
}

void genericinstance::solveik() {
	switch (mmodelsettings.msikmode) {
	case ikmode::ccd:
		solveikbyccd(mmodelsettings.msiktargetworldpos);
		break;
	case ikmode::fabrik:
		solveikbyfabrik(mmodelsettings.msiktargetworldpos);
		break;
	default:
		break;
	}
}

void genericinstance::playanimation(int animNum, float speedDivider, float blendFactor, replaydirection direction) {
	double currentTime =
	    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
	    .count();
	if (mmodelsettings.animloop)
		mmodelsettings.msanimtimepos = std::fmod((currentTime - mmodelsettings.animstart) / 1000.0 * speedDivider,
		                               manimclips.at(animNum)->getEndTime());
	else
		mmodelsettings.msanimtimepos = (currentTime - mmodelsettings.animstart) / 1000.0 * speedDivider;
	if (direction == replaydirection::backward) {
		blendanimationframe(animNum, mmodelsettings.msanimtimepos, blendFactor);
	} else {
		blendanimationframe(animNum, mmodelsettings.msanimtimepos, blendFactor);
	}
}

void genericinstance::playanimation(int sourceAnimNumber, int destAnimNumber, float speedDivider, float blendFactor,
                                    replaydirection direction) {
	double currentTime =
	    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
	    .count();

	if (direction == replaydirection::backward) {
		crossblendanimationframe(
		    sourceAnimNumber, destAnimNumber,
		    manimclips.at(sourceAnimNumber)->getEndTime() -
		    std::fmod(currentTime / 1000.0 * speedDivider, manimclips.at(sourceAnimNumber)->getEndTime()),
		    blendFactor);
	} else {
		if (!mmodelsettings.animloop) {
			currentTime -= mmodelsettings.animstart;
			if (currentTime < manimclips.at(sourceAnimNumber)->getEndTime())
				crossblendanimationframe(
				    sourceAnimNumber, destAnimNumber,
				    std::fmod(currentTime / 1000.0 * speedDivider, manimclips.at(sourceAnimNumber)->getEndTime()), blendFactor);
		} else {
			crossblendanimationframe(
			    sourceAnimNumber, destAnimNumber,
			    std::fmod(currentTime / 1000.0 * speedDivider, manimclips.at(sourceAnimNumber)->getEndTime()), blendFactor);
		}
	}
}

void genericinstance::blendanimationframe(int animNum, float time, float blendFactor) {
	manimclips.at(animNum)->blendFrame(mnodelist, madditiveanimationmask, time, blendFactor);
	updatenodematrices(mrootnode);
}

void genericinstance::crossblendanimationframe(int sourceAnimNumber, int destAnimNumber, float time,
        float blendFactor) {

	float sourceAnimDuration = manimclips.at(sourceAnimNumber)->getEndTime();
	float destAnimDuration = manimclips.at(destAnimNumber)->getEndTime();

	float scaledTime = time * (destAnimDuration / sourceAnimDuration);

	manimclips.at(sourceAnimNumber)->setFrame(mnodelist, madditiveanimationmask, time);
	manimclips.at(destAnimNumber)->blendFrame(mnodelist, madditiveanimationmask, scaledTime, blendFactor);

	manimclips.at(destAnimNumber)->setFrame(mnodelist, minvertedadditiveanimationmask, scaledTime);
	manimclips.at(sourceAnimNumber)->blendFrame(mnodelist, minvertedadditiveanimationmask, time, blendFactor);

	updatenodematrices(mrootnode);
}

void genericinstance::updateadditivemask(std::shared_ptr<vknode> treeNode, int splitNodeNum) {
	/* break chain here */
	if (treeNode->getnum() == splitNodeNum) {
		return;
	}

	madditiveanimationmask.at(treeNode->getnum()) = false;
	for (auto &childNode : treeNode->getchildren()) {
		updateadditivemask(childNode, splitNodeNum);
	}
}

void genericinstance::setskeletonsplitnode(int nodeNum) {
	std::fill(madditiveanimationmask.begin(), madditiveanimationmask.end(), true);
	updateadditivemask(mrootnode, nodeNum);

	minvertedadditiveanimationmask = madditiveanimationmask;
	minvertedadditiveanimationmask.flip();
}

void genericinstance::setinstancesettings(modelsettings &settings) {
	mmodelsettings = settings;
}

modelsettings &genericinstance::getinstancesettings() {
	return mmodelsettings;
}

glm::vec3 genericinstance::getwpos() {
	return mmodelsettings.msworldpos;
}

glm::quat genericinstance::getwrot() {
	return glm::normalize(
	           glm::quat(glm::vec3(glm::radians(mmodelsettings.msworldrot.x), glm::radians(mmodelsettings.msworldrot.y),
	                               glm::radians(mmodelsettings.msworldrot.z))));
}

float genericinstance::getanimendtime(int animNum) {
	return manimclips.at(animNum)->getEndTime();
}

void genericinstance::setinversekindematicsnode(int effectorNodeNum, int ikChainRootNodeNum) {
	if (effectorNodeNum < 0 || effectorNodeNum > (mnodelist.size() - 1)) {
		return;
	}

	if (ikChainRootNodeNum < 0 || ikChainRootNodeNum > (mnodelist.size() - 1)) {
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
			} else {
				/* force stopping on the root node */
				break;
			}
		}
	}

	miksolver.setnodes(ikNodes);
}

void genericinstance::setnumikiterations(int iterations) {
	miksolver.setnumiterations(iterations);
}

glm::vec3 *genericinstance::getinstpos() {
	return &mmodelsettings.msworldpos;
}

void genericinstance::solveikbyccd(glm::vec3 target) {
	miksolver.solveccd(target);
	updatenodematrices(miksolver.getikchainrootnode());
}

void genericinstance::solveikbyfabrik(glm::vec3 target) {
	miksolver.solvefabrik(target);
	updatenodematrices(miksolver.getikchainrootnode());
}
