#include "gamestate.hpp"


#include <iostream>

void gamestate::tick(){
	tickstart = std::chrono::high_resolution_clock::now();
	gameticknum++;

	checkstate();

	if (!checkplayerhp())mstate = gamestate0::dead;
	checkenemies();

	for (auto& i : *spells) {
		if (!i->ready) {
			i->ready = checkcooldown(i);
		}
		if (i->active) {
			for (const auto& j : (*menemies)) {
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


	if (!mnobjs->offlineplay) {
		moveothers();
		if (mnobjs->rdserverclient) {
			for (const auto& i : mnobjs->nserver->GetConnectedClients()) {
				//mnobjs->nserver->sendgamepos(i.first, 0, (*mplayer)->getinst(0)->getinstancesettings().msworldpos,false);
				for (const auto& j : aposes) {
					if(j.first!=0 && j.first!=i.first)
						mnobjs->nserver->sendgamepos(i.first, j.first, j.second);
					else if(j.first != 0)
						mnobjs->nserver->sendgamepos(i.first, i.first, (*mplayer)->getinst(0)->getinstancesettings().msworldpos);
				}
			}
		} else {
			mnobjs->nclient->sendgamepos((*mplayer)->getinst(0)->getinstancesettings().msworldpos,false);
		}
	}

	

	auto x = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tickstart);

	if (x < std::chrono::microseconds(8)) {
		//std::cout << x << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(8) - x);
	}
}


void gamestate::init(vkobjs& objs,netobjs& nobjs, std::function<void()> mvplayer, std::function<void()> mvenemies,std::shared_ptr<playoutplayer>& p, std::shared_ptr<playoutcircle>& c, std::vector<std::shared_ptr<playoutmodel>>& e, std::vector<std::shared_ptr<spell>>& s, std::shared_ptr<playoutcircle>& bars, std::unordered_map<ClientID,std::shared_ptr<playoutplayer>>& o) {

	mvkobjs = &objs;
	mnobjs = &nobjs;

	moveplayer = mvplayer;
	moveenemies = mvenemies;

	mplayer=&p;
	mcircle = &c;
	menemies = &e;
	spells=&s;
	mbars = &bars;
	others = &o;


	spellcasting.insert_or_assign(spells->at(0)->spellid, [&]() {
			staticsettings& s = (*mcircle)->getinst(0)->getinstancesettings();
			s.msworldpos = mvkobjs->raymarchpos;
			s.msworldpos.y += 12.0f;
			s.msworldscale = glm::vec3{ 500.0f };
			s.msworldrot.x = -90.0f;
			s.msdrawmodel = true;
			spells->at(0)->size = 500;
			spells->at(0)->pos = mvkobjs->raymarchpos;
		});

	spellcasting.insert_or_assign(spells->at(1)->spellid, [&]() {
		(*mplayer)->freezedecay();
		(*mplayer)->createdecayinstances(*mvkobjs);
		modelsettings& s = (*mplayer)->getinst(0)->getinstancesettings();
		//modelsettings& d = mplayer->getdecayinst(0)->getinstancesettings();
		//d.msworldpos = s.msworldpos;
		s.msworldpos = (*mvkobjs).raymarchpos;
		s.msanimclip = 2;

		});

	spellfading.insert_or_assign(spells->at(0)->spellid, [&]() {
		staticsettings& s = (*mcircle)->getinst(0)->getinstancesettings();
		s.msworldpos = glm::vec3{ -100.0f };
		spells->at(0)->pos = s.msworldpos;
		});

	spellfading.insert_or_assign(spells->at(1)->spellid, [&]() {

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
	if ((*mplayer)->getinst(0)->getinstancesettings().hp < 0.0) {
		(*mplayer)->getinst(0)->getinstancesettings().dead = true;
		return false;
	}
	else
		return true;
}
void gamestate::checkenemies() {
	alldead = false;
	for (size_t i{ 0 }; i < (*menemies).size(); i++) {
		for (size_t j{ 0 }; j < (*menemies)[i]->getnuminstances(); j++) {


			modelsettings& s = (*menemies)[i]->getinst(j)->getinstancesettings();

			if (!s.dead)alldead = true;

			if (s.hp < 0.0 && !s.dead) {
				s.msanimclip = 1;
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
	if ((*mplayer)->getinst(0)->getinstancesettings().dead)
		mstate = gamestate0::dead;
	else if (std::all_of((*menemies).begin(), (*menemies).end(), [&](std::shared_ptr<playoutmodel> x) {
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
void gamestate::moveothers(){
	for (const auto& i : (*others)) {
		if(i.first!=0)
			if (i.second->ready) {
				modelsettings& s = i.second->getinst(0)->getinstancesettings();
				if (glm::distance(s.msworldpos, aposes.at(i.first)) < 0.01f) {
					standingticks[i.first]++;
					if(standingticks[i.first]>20)
						s.msanimclip = 2;
				} else {
					standingticks[i.first] = 0;
					glm::vec3 diff = glm::normalize(aposes.at(i.first) - s.msworldpos);
					s.msworldrot.y = glm::degrees(glm::atan(diff.x, diff.z));
					s.msanimclip = 3;
				}
				s.msworldpos = aposes.at(i.first);
			}
	}
}
void gamestate::addplayer(ClientID id){
	if ((*others).find(id) == (*others).end()) {
		(*others).insert_or_assign(id, std::make_shared<playoutplayer>());
		(*others)[id]->setup(*mvkobjs, "resources/t0.glb", 1);
		(*others)[id]->setup2(*mvkobjs, "shaders/player_nocape.vert.spv", "shaders/player_nocape.frag.spv");
		standingticks.insert_or_assign(id, 0);
	}
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
	(*mplayer)->getinst(0)->getinstancesettings().hp = 1.0;
	(*mplayer)->getinst(0)->getinstancesettings().msworldpos = glm::vec3{ 0.0f };
	(*mplayer)->getinst(0)->getinstancesettings().dead = false;

	for (size_t i{ 0 }; i < (*menemies).size(); i++) {
		for (size_t j{ 0 }; j < (*menemies)[i]->getnuminstances(); j++) {
			(*menemies)[i]->getinst(j)->getinstancesettings().hp = 1.0;
			(*menemies)[i]->getinst(j)->getinstancesettings().msworldpos = glm::vec3{ std::rand() % 5999,0.0f,std::rand() % 5999 };

			(*menemies)[i]->getinst(j)->getinstancesettings().dead = false;

			(*menemies)[i]->getinst(j)->getinstancesettings().animloop = true;
		}
	}

	//std::transform(mbars->getallinstances().begin(), mbars->getallinstances().end(), mbars->getallinstances().begin(), [&](std::shared_ptr<texinstance>& x) { x->getinstancesettings().msworldpos.y = 200; });
	std::for_each((*mbars)->getallinstances().begin(), (*mbars)->getallinstances().end(), [&](std::shared_ptr<texinstance>& x) { x->getinstancesettings().msworldpos.y = 200; });
	

	mstate = gamestate0::menu;
}
