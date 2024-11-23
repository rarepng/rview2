#include "gamestate.hpp"


#include <iostream>

void gamestate::tick(){
	tickstart = std::chrono::high_resolution_clock::now();
	gameticknum++;

	checkstate();

	if (!checkplayerhp())mstate = gamestate0::dead;
	checkenemies();

	for (auto& i : spells) {
		if (!i->ready) {
			i->ready = checkcooldown(i);
		}
		if (i->active) {
			for (const auto& j : menemies) {
				for (size_t k{ 0 }; k < j->getnuminstances(); k++) {
					modelsettings& s = j->getinst(k)->getinstancesettings();
					if (intersercthitbox(s.msworldpos, std::pair<glm::vec3, unsigned int>{i->pos, i->size})) {
						s.hp -= i->dmg;
					}
				}
			}
			i->active = checkactivity(i);
			if (!i->active) {
				spellfading[i->spellid]();
			}
		}
		if (i->cast) {
			i->active = true;
			i->ready = false;
			i->cdelapsed = 0;
			i->activeelapsed = 0;
			cast(i);
			i->cast = false;
		}
	}

	moveplayer();
	moveenemies();




	auto x = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tickstart);

	if (x < std::chrono::microseconds(8)) {
		std::cout << x << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(8) - x);
	}
}


void gamestate::init(vkobjs& objs, std::function<void()> mvplayer, std::function<void()> mvenemies,std::shared_ptr<playoutplayer>& p, std::shared_ptr<playoutcircle>& c, std::vector<std::shared_ptr<playoutmodel>>& e, std::vector<std::shared_ptr<spell>>& s) {

	mvkobjs = &objs;

	moveplayer = mvplayer;
	moveenemies = mvenemies;

	mplayer=p;
	mcircle = c;
	menemies = e;
	spells=s;


	spellcasting.emplace(spells[0]->spellid, [&]() {
			mcircle->createinstances(objs, 1, false);
			staticsettings& s = mcircle->getinst(0)->getinstancesettings();
			s.msworldpos = mvkobjs->raymarchpos;
			s.msworldpos.y += 12.0f;
			s.msworldpos.z += 500.0f;
			s.msworldpos.x -= 500.0f;
			s.msworldscale = glm::vec3{ 1000.0f };
			s.msworldrot.x = -90.0f;
			s.msdrawmodel = true;
			spells[0]->size = 500;
			spells[0]->pos = mvkobjs->raymarchpos;
		});

	spellcasting.emplace(spells[1]->spellid, [&]() {
		mplayer->freezedecay();
		mplayer->createdecayinstances(*mvkobjs);
		modelsettings& s = mplayer->getinst(0)->getinstancesettings();
		//modelsettings& d = mplayer->getdecayinst(0)->getinstancesettings();
		//d.msworldpos = s.msworldpos;
		s.msworldpos = (*mvkobjs).raymarchpos;
		s.msanimclip = 9;

		});

	spellfading.emplace(spells[0]->spellid, [&]() {
		});

	spellfading.emplace(spells[1]->spellid, [&]() {

		});



}

void gamestate::wavelogic() {

}

bool gamestate::checkcooldown(std::shared_ptr<spell>& s){
	if (s->cdelapsed > s->cooldownticks)return true;
	s->cdelapsed++;
	return false;
}

bool gamestate::checkactivity(std::shared_ptr<spell>& s){
	if (s->activeelapsed > s->activeticks)return false;
	s->activeelapsed++;
	return true;
}

bool gamestate::checkplayerhp(){
	if (mplayer->getinst(0)->getinstancesettings().hp < 0.0) {
		mplayer->getinst(0)->getinstancesettings().dead = true;
		return false;
	}
	else
		return true;
}
void gamestate::checkenemies() {
	alldead = false;
	for (size_t i{ 0 }; i < menemies.size(); i++) {
		for (size_t j{ 0 }; j < menemies[i]->getnuminstances(); j++) {


			modelsettings& s = menemies[i]->getinst(j)->getinstancesettings();

			if (!s.dead)alldead = true;

			if (s.hp < 0.0 && !s.dead) {
				s.msanimclip = 6;
				s.animloop = false;
				s.msanimspeed = 0.5f;
				s.msanimtimepos = 0.0f;
				s.animstart = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
				s.dead = true;
			}
		}
	}
}
void gamestate::checkstate(){
	if (mplayer->getinst(0)->getinstancesettings().dead)
		mstate = gamestate0::dead;
	else if (std::all_of(menemies.begin(), menemies.end(), [&](std::shared_ptr<playoutmodel> x) {
		const auto& y = x->getallinstances();
		return std::all_of(y.begin(), y.end(), [&](std::shared_ptr<animinstance> z) {
			return z->getinstancesettings().dead;
			});
		})) {
		mstate = gamestate0::won;
	}
}
gamestate0 gamestate::getstate(){
	return mstate;
}
pausestate gamestate::getpause(){
	return mpause;
}
void gamestate::setstate(gamestate0 state){
	mstate = state;
}
void gamestate::setpause(pausestate state){
	mpause = state;
}
void gamestate::cast(std::shared_ptr<spell>& s){
	spellcasting[s->spellid]();
}
bool gamestate::intersercthitbox(glm::vec3 x, std::pair<glm::vec3, unsigned int> y) {
	if (x.x < y.first.x + y.second && x.x > y.first.x - y.second && x.z < y.first.z + y.second && x.z > y.first.z - y.second)
		return true;
	else
		return false;
}


void gamestate::resetwave() {
	mplayer->getinst(0)->getinstancesettings().hp = 1.0;
	mplayer->getinst(0)->getinstancesettings().msworldpos = glm::vec3{ 0.0f };
	mplayer->getinst(0)->getinstancesettings().dead = false;

	for (size_t i{ 0 }; i < menemies.size(); i++) {
		for (size_t j{ 0 }; j < menemies[i]->getnuminstances(); j++) {
			menemies[i]->getinst(j)->getinstancesettings().hp = 1.0;
			menemies[i]->getinst(j)->getinstancesettings().msworldpos = glm::vec3{ std::rand() % 5999,0.0f,std::rand() % 5999 };

			menemies[i]->getinst(j)->getinstancesettings().dead = false;

			menemies[i]->getinst(j)->getinstancesettings().animloop = true;



		}
	}

	mstate = gamestate0::menu;
}
