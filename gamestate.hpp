#pragma once

#include <chrono>
#include <vector>
#include <memory>
#include <functional>
#include <thread>

#include "vkobjs.hpp"
#include "playoutplayer.hpp"
#include "playoutcircle.hpp"
#include "playoutmodel.hpp"

class gamestate {
public:
	static void tick();
	static bool intersercthitbox(glm::vec3 x, std::pair<glm::vec3, unsigned int> y);
	static void init(vkobjs& objs, std::function<void()> mvplayer, std::function<void()> mvenemies , std::shared_ptr<playoutplayer>& p, std::shared_ptr<playoutcircle>& c, std::vector<std::shared_ptr<playoutmodel>>& e, std::vector<std::shared_ptr<spell>>& s);
	static void wavelogic();
	static bool checkcooldown(std::shared_ptr<spell>& s);
	static bool checkactivity(std::shared_ptr<spell>& s);
	static bool checkplayerhp();
	static void checkenemies();
	static void checkstate();
	static gamestate0 getstate();
	static pausestate getpause();
	static void setstate(gamestate0 state);
	static void setpause(pausestate state);

	static void resetwave();

	



private:
	static void cast(std::shared_ptr<spell>& s);


	inline static std::function<void()> moveplayer;
	inline static std::function<void()> moveenemies;

	inline static bool alldead{false};

	inline static std::map< const unsigned int, std::function<void()>> spellcasting;
	inline static std::map< const unsigned int, std::function<void()>> spellfading;
	//inline static std::map<unsigned int, std::function<void(vkobjs& objs, glm::vec3& pos, std::shared_ptr<playoutcircle>& c)>> spellactivity{
	//	{0,[&](vkobjs& objs,glm::vec3& pos,std::shared_ptr<playoutcircle>& c) {
	//		c->createinstances(objs, 1, false);
	//		staticsettings& s = c->getinst(0)->getinstancesettings();
	//		s.msworldpos = pos;
	//		s.msworldpos.y += 12.0f;
	//		s.msworldpos.z += 500.0f;
	//		s.msworldpos.x -= 500.0f;
	//		s.msworldscale = glm::vec3{ 1000.0f };
	//		s.msworldrot.x = -90.0f;
	//		s.msdrawmodel = true;
	//	}}
	//};
	inline static vkobjs* mvkobjs;
	inline static std::shared_ptr<playoutplayer> mplayer{};
	inline static std::shared_ptr<playoutcircle> mcircle;
	inline static std::vector<std::shared_ptr<playoutmodel>> menemies;
	inline static std::vector<std::shared_ptr<spell>> spells;
	inline static std::vector<spell> activespells{};
	inline static std::vector<spell> coolingdownspells{};
	inline static size_t gameticknum{ 0 };
	inline static std::chrono::high_resolution_clock::time_point tickstart{};
	inline static gamestate0 mstate{ gamestate0::menu };
	inline static gamestage mstage{ gamestage::combat };
	inline static wavetype mwave{ wavetype::horde };
	inline static pausestate mpause{ pausestate::resumed };
};
