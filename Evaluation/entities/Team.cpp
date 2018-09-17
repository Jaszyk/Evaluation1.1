#include "Actor.h"
#include "Team.h"

Team::Team(unsigned short number) : _teamNumber(number) {}

unsigned short Team::getNumber() const { return _teamNumber; }

size_t Team::getSize() const { return _members.size(); }

SDL_Color Team::getColor() const { return _color; }

bool Team::hasMember(const String& name) const { return _members.find(name) != _members.end(); }

std::vector<Actor*> Team::getMembers() const {
	std::vector<Actor*> result;
	result.reserve(_members.size());
	for (auto it = _members.begin(); it != _members.end(); ++it) {
		result.push_back(it->second);
	}
	return result;
}

Actor* Team::getMember(const String& name) const {
	auto it = _members.find(name);
	return it != _members.end() ? it->second : nullptr;
}

void Team::addMember(const String& name, Actor* member) {
	if (!hasMember(name)) {
		_members[name] = member;
		member->_team = this;
	}
}

size_t Team::getRemainingActors() const {
	size_t alive = 0;
	for (auto entry : _members) {
		if (!entry.second->isDead()) {
			alive++;
		}
	}
	return alive;
}

 float Team::getTotalRemainingHelath() const {
	float health = 0;
	for (auto entry : _members) {
		health += entry.second->getHealth();
	}
	return health;
}