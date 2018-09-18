#include <vector>
#include "Actor.h"
#include "Missile.h"
#include "Team.h"
#include "Trigger.h"
#include "actions/Action.h"
#include "engine/MissileManager.h"
#include "engine/CommonFunctions.h"
#include "main/Configuration.h"
#include "math/Math.h"
#include "entities//Weapon.h"
#include "main/Game.h"
#include "entities/Wall.h"

Actor::Actor(const std::string& name, const Vector2& position)
	: GameDynamicObject(position, common::PI_F / 2) {
	_name = name;
	_health = common::min(Config.ActorMaxHealth, Config.ActorMaxHealth * Config.ActorInitialHealth);
	_armor = 0;
	_armorShotsRemaining = 0;
	_velocity = Vector2();
	_rotation = 0;
	_isRotating = false;
	_isWaiting = false;
	_currentWeapon = Config.DefaultWeapon;
	_recalculations = 0;
	_positionHistoryLength = 0;
	_nextHistoryIdx = 0;

	for (size_t i = 0; i < Config.ActionPositionHistoryLength; ++i) {
		_positionHistory.push_back(_position);
	}

	for (auto entry : MissileManager::getWeaponsInfo()) {
		WeaponState weaponState;
		weaponState.lastShot = 0;
		weaponState.state = WeaponLoadState::WEAPON_LOADED;
		weaponState.ammo = entry.second.initialAmmo;
		_weapons[entry.first] = weaponState;
	}
}

Actor::~Actor() { }

float Actor::getHealth() const { return _health; }

String Actor::getCurrentWeapon() const { return _currentWeapon; }

Vector2 Actor::getDestination() const { return _lastDestination; }

String Actor::getName() const { return _name; }

Team* Actor::getTeam() const { return _team; }

float Actor::getArmor() const { return _armor; }

int Actor::getRemainingArmorShots() const { return _armorShotsRemaining; }

std::vector<GameDynamicObject*> Actor::getSeenObjects() const { return _nearbyObjects; }

bool Actor::isDead() const { return _currentAction != nullptr && _currentAction->getActionType() == ActionType::DEAD; }

float Actor::getMaxSpeed() const { return Config.ActorSpeed; }

bool Actor::isMoving() const { return _path.size() > 0 || _preferredVelocity.lengthSquared() > common::EPSILON; }

bool Actor::isRotating() const { return _isRotating; }

bool Actor::hasPositionChanged() const { return _velocity.lengthSquared() > common::EPSILON; }

void Actor::setAmmo(const String& weaponName, int value) { _weapons[weaponName].ammo = value; }

void Actor::setArmor(float value) { _armor = value; }

void Actor::setRemainingArmorShots(int value) { _armorShotsRemaining = value; }

void Actor::setPreferredVelocity(const Vector2& velocity) { _preferredVelocity = velocity; }

Vector2 Actor::getPreferredVelocity() const { return _preferredVelocity; }

Vector2 Actor::getVelocity() const { return _velocity; }

Action* Actor::getCurrentAction() const { return _currentAction; }

Vector2 Actor::getShortGoal() const { return _nextSafeGoal; }

Vector2 Actor::getLongGoal() const { return _path.empty() ? _position : _path.back(); }

float Actor::estimateRemainingDistance() const {
	auto path = this->getCurrentPath();
	float distance = 0;
	if (!path.empty()) {
		Vector2 last = path.front();
		path.pop(); 
		Vector2 next;
		while (!path.empty()) {
			next = path.front();
			path.pop();
			distance += common::distance(last, next);
			next = last;
		}
	}
	return distance;
}

void Actor::clearCurrentAction() {
	if (_currentAction != nullptr) {
		delete _currentAction; 
		_currentAction = nullptr;
	}
}

bool Actor::setCurrentAction(Action* action) {
	if (_currentAction != nullptr) {
		if (_currentAction->getPriority() <= action->getPriority()) {
			_currentAction->finish(0);
			delete _currentAction;
			_currentAction = action;
			return true;
		}
		return false;
	}
	else {
		_currentAction = action;
		return true;
	}	
}

void Actor::setNextAction(Action* action) {
	if (_nextAction != nullptr) {
		delete _nextAction;
	}
	_nextAction = action;
}

float Actor::damage(float dmg) {
	if (dmg < _health) {
		_health -= dmg;
	}
	else {
		dmg = _health;
		_health = 0;
	}
	Logger::log("Actor " + _name + " receives " + std::to_string(dmg) + " damage.");
	return dmg;
}

float Actor::heal(float health) {
	if (isDead()) { return 0; }
	if (_health + health > Config.ActorMaxHealth) {
		health += _health - Config.ActorMaxHealth;
		_health = Config.ActorMaxHealth;
	}
	else {
		_health += health;
	}
	Logger::log("Actor " + _name + " restores " + std::to_string(health) + " health.");
	return health;
}

void Actor::move(const std::queue<Vector2>& path) { 
	Logger::log("Actor " + _name + " chose new destination.");
	if (!path.empty()) {
		_path = path;
		_lastDestination = path.empty() ? _position : path.back();
		_nextSafeGoal = getNextSafeGoal();
	}
	else {
		abortMovement("Actor " + _name + " can't reach this destination.", true);
	}
}

void Actor::abortMovement(String loggerMessage, bool resetCounter) {
	_path = {};
	_preferredVelocity = Vector2();
	_isWaiting = false;
	_nextSafeGoal = _position;
	_lastDestination = _position;
	_positionHistoryLength = 0;
	_nextHistoryIdx = 0;
	if (resetCounter) {
		_recalculations = 0;
	}
	clearPositionHistory();
	Logger::log(loggerMessage);
}

void Actor::stop() {
	abortMovement("Actor " + _name + " stopped.", true);
}

Aabb Actor::getAabb() const {
	float r = Config.ActorRadius;
	return Aabb(
		_position.x - r, 
		_position.y - r, 
		2 * r, 
		2 * r);
}

float Actor::getRadius() const { return Config.ActorRadius; }

bool Actor::isSolid() const { return true; }

void Actor::registerKill(Actor* actor) {
	if (actor->_team != this->_team) { this->_kills++; }
	else { this->_friendkills++; }
	std::cout << "Actor " << actor->_name << " was killed by actor " << _name << ".\n";
}

void Actor::setCurrentWeapon(const String& weaponName) {
	if (_currentWeapon != weaponName) {
		_currentWeapon = weaponName;
		Logger::log("Actor " + _name + " changed weapon to " + weaponName + ".");
	}
}

WeaponState& Actor::getWeaponState(const String& weaponName) { return _weapons[weaponName]; }

const WeaponState& Actor::getWeaponState(const String& weaponName) const { return _weapons.at(weaponName); }

GameDynamicObjectType Actor::getGameObjectType() const { return GameDynamicObjectType::ACTOR; }

void Actor::lookAt(const Vector2& point) {
	_desiredOrientation = common::angleFromTo(_position, point);
	_isRotating = true;
}

bool Actor::updateCurrentAction(GameTime time) {
	if (_currentAction != nullptr) {
		if (!_currentAction->hasStarted()) {
			_currentAction->start(time);
		}
		if (_currentAction->update(time)) {
			_currentAction->finish(time);
			clearCurrentAction();
			if (_nextAction != nullptr) {
				_currentAction = _nextAction;
				_nextAction = nullptr;
			}
		}
		return true;
	}
	return false;
}

void Actor::update(GameTime time) {	
#ifdef _DEBUG
	GameTime from, to;
	bool logIfSuccessful;

	from = SDL_GetPerformanceCounter();
	logIfSuccessful = updateWeapons(time);
	to = SDL_GetPerformanceCounter();
	if (logIfSuccessful) {
		Logger::logIfNotIgnored("updateWeapons", "Update Weapons:         " + std::to_string(to - from));
	}

	from = SDL_GetPerformanceCounter();
	logIfSuccessful = updateCurrentAction(time);
	to = SDL_GetPerformanceCounter();
	if (logIfSuccessful) {
		Logger::logIfNotIgnored("updateCurrentAction", "Update Current Action:  " + std::to_string(to - from));
	}

	updateMovement(time);

	from = SDL_GetPerformanceCounter();
	logIfSuccessful = updateOrientation(time);
	to = SDL_GetPerformanceCounter();
	if (logIfSuccessful) {
		Logger::logIfNotIgnored("updateOrientation", "Update Orientation:     " + std::to_string(to - from));
	}
#else
	updateWeapons(time);
	updateCurrentAction(time);
	updateMovement(time);
	updateOrientation(time);
#endif 

	GameDynamicObject::update(time);
}

float Actor::calculateRotation() const {
	bool clockwise = common::isAngleBetween(_desiredOrientation, _orientation, _orientation + common::PI_F);
	float diff = clockwise ? common::measureAngle(_orientation, _desiredOrientation)
		: common::measureAngle(_desiredOrientation, _orientation);
	if (diff > common::PI_F) { return 0; }
	if (diff < Config.ActorRotationSpeed) { return (clockwise ? diff : -diff) / Config.ActorRotationSpeed; }
	return clockwise ? 1.0f : -1.0f;
}

void Actor::setPreferredVelocityAndSafeGoal() {
	if (!_path.empty()) {
		Vector2 toDestination = _path.back() - _position;
		if (Game::getInstance()->getMap()->isMovementValid(this, toDestination)) {
			if ((toDestination).lengthSquared() < common::sqr(Config.MovementGoalMargin)) {
				abortMovement("Actor " + _name + " reached its destination.", true);
			}
			else {
				_preferredVelocity = toDestination;
			}
		}
		else {
			if ((_nextSafeGoal - _position).lengthSquared() < common::sqr(Config.MovementGoalMargin)) {
				_path.pop();
				_nextSafeGoal = getNextSafeGoal();
			}
			if (!_path.empty()) {
				_preferredVelocity = _nextSafeGoal - _position;
			}
			else {
				abortMovement("Actor " + _name + " reached its destination.", true);
			}
		}
	}
}

bool Actor::updateOrientation(GameTime time) {
	if (isRotating()) {
		_orientation = common::normalizeAngle(_orientation);
		_rotation = calculateRotation();
		_orientation += _rotation * Config.ActorRotationSpeed;
		if (common::abs(_orientation - _desiredOrientation) < common::EPSILON) {
			_isRotating = false;
		}
		return true;
	}
	return false;
}

void Actor::updateMovement(GameTime time) {
	if (isMoving()) {

#ifdef _DEBUG
		GameTime from, to;

		from = SDL_GetPerformanceCounter();
		setPreferredVelocityAndSafeGoal();
		to = SDL_GetPerformanceCounter();
		Logger::logIfNotIgnored("setPreferredVelocityAndSafeGoal", "Compute Pref. Velocity: " + std::to_string(to - from));		

		from = SDL_GetPerformanceCounter();
		updateSpotting();
		to = SDL_GetPerformanceCounter();
		Logger::logIfNotIgnored("updateSpotting", "Update spotting:        " + std::to_string(to - from));

		from = SDL_GetPerformanceCounter();
		auto actorsInViewAngle = getObjectsInViewAngle();
		to = SDL_GetPerformanceCounter();
		Logger::logIfNotIgnored("getActorsInViewAngle", "Actors in View Angle:   " + std::to_string(to - from));

		from = SDL_GetPerformanceCounter();
		auto velocityObstacles = getVelocityObstacles(actorsInViewAngle);
		to = SDL_GetPerformanceCounter();
		Logger::logIfNotIgnored("getVelocityObstacles", "Get Velocity Obstacles: " + std::to_string(to - from));

		from = SDL_GetPerformanceCounter();
		auto candidates = computeCandidates(velocityObstacles);
		to = SDL_GetPerformanceCounter();
		Logger::logIfNotIgnored("computeCandidates", "Compute Candidates:     " + std::to_string(to - from));

		from = SDL_GetPerformanceCounter();
		_velocity = selectVelocity(candidates);
		to = SDL_GetPerformanceCounter();
		Logger::logIfNotIgnored("selectVelocity", "Select Velocity:        " + std::to_string(to - from));

		if (_velocity.lengthSquared() > common::EPSILON) {
			_velocity = _velocity.normal() * getMaxSpeed();
		}

		from = SDL_GetPerformanceCounter();
		MovementCheckResult movementCheckResult = checkMovement();
		to = SDL_GetPerformanceCounter();
		Logger::logIfNotIgnored("checkMovement", "Check Movement:         " + std::to_string(to - from));

		bool oscilationDetected = isOscilating();

		from = SDL_GetPerformanceCounter();
#else		
		setPreferredVelocityAndSafeGoal();
		updateSpotting();
		selectVelocity(computeCandidates(getVelocityObstacles(getObjectsInViewAngle())));
		
		if (_velocity.lengthSquared() > common::EPSILON) {
			_velocity = _velocity.normal() * getMaxSpeed();
		}

		MovementCheckResult movementCheckResult = checkMovement();
		
		bool oscilationDetected = isOscilating();
#endif
		if (movementCheckResult.allowed && !oscilationDetected) {
			_position += _velocity;
			saveCurrentPositionInHistory();
			_isWaiting = false;

			if ((_currentAction == nullptr || !_currentAction->locksRotation())
				&&_velocity.lengthSquared() > common::EPSILON) {
				_desiredOrientation = common::angle(_velocity);
				_isRotating = true;
			}

			for (Trigger* trigger : movementCheckResult.triggers) {
				if (trigger->isActive()) { trigger->pick(this, time); }
			}
		}
		else if (_isWaiting || oscilationDetected) {
			if (time - _waitingStarted > (_recalculations > 0 ? Config.MaxRecalculatedWaitingTime : Config.MaxMovementWaitingTime)) {
				if (!_path.empty()) {
					Vector2 destination = _path.back();
					abortMovement("Actor " + _name + " is searching for alternative path.", false);
					if (_recalculations < Config.MaxRecalculations) {
						++_recalculations;
						move(Game::getInstance()->getMap()->findPath(_position, destination, this, {
							common::Circle(_position, Config.ActorRadius * (_recalculations + 1))
						}));
					}
				}
				else {
					stop();
				}
			}
		}
		else {
			Logger::log("Actor " + _name + " is waiting.");
			_isWaiting = true;
			_waitingStarted = time;
			_velocity.x = 0;
			_velocity.y = 0;
		}
#ifdef  _DEBUG
		to = SDL_GetPerformanceCounter();
		Logger::logIfNotIgnored("updateMovement", "Update Movement:        " + std::to_string(to - from));
#endif 		
	}
}

void Actor::saveCurrentPositionInHistory() {
	++_nextHistoryIdx;
	if (_nextHistoryIdx == Config.ActionPositionHistoryLength) {
		_nextHistoryIdx = 0;
	}
	if (_positionHistoryLength < Config.ActionPositionHistoryLength) {
		++_positionHistoryLength;
	}
	_positionHistory[_nextHistoryIdx].x = _position.x;
	_positionHistory[_nextHistoryIdx].y = _position.y;
}

bool Actor::isOscilating() const {
	if (_positionHistoryLength == Config.ActionPositionHistoryLength) {
		Vector2 pos = _positionHistory[0];
		float margin = common::sqr(Config.ActorOscilationRadius);
		for (int i = 1; i < _positionHistoryLength; ++i) {
			if (common::sqDist(pos, _positionHistory[i]) > margin) {
				return false;
			}
		}
		Logger::log("Oscilation detected!");
		return true;
	}
	return false;
}

void Actor::clearPositionHistory() {
	float x = _position.x;
	float y = _position.y;
	size_t n = Config.ActionPositionHistoryLength;
	for (int i = 0; i < n; ++i) {
		_positionHistory[i].x = x;
		_positionHistory[i].y = y;
	}
}

bool Actor::updateWeapons(GameTime time) {
	bool wasAntyhingUpdated = false;
	for (auto entry : MissileManager::getWeaponsInfo()) {
		const String& weaponName = entry.first;
		if (_weapons[weaponName].state == WeaponLoadState::WEAPON_UNLOADED
			&& time - _weapons[weaponName].lastShot
			> entry.second.reloadTime) {
			_weapons[weaponName].state = WeaponLoadState::WEAPON_LOADED;
			wasAntyhingUpdated = true;
		}
	}
	return wasAntyhingUpdated;
}

Actor::MovementCheckResult Actor::checkMovement() const {
	Vector2 futurePosition = _position + _velocity;
	float r = Config.ActorRadius;
	auto potentialColliders = getDynamicObjectsInArea(futurePosition, r);

	Actor::MovementCheckResult result;
	result.allowed = true;
	result.triggers = std::vector<Trigger*>();

	common::Circle futureCollisionArea = { futurePosition, r };

	for (GameDynamicObject* entity : potentialColliders) {
		if (entity != this && common::testCircles(futureCollisionArea, entity->getCollisionArea())) {
			if (entity->isSolid()) { result.allowed = false; }
			if (entity->getGameObjectType() == GameDynamicObjectType::TRIGGER) { result.triggers.push_back((Trigger*)entity); }
		}
	}

	if (checkMovementCollisions(Segment(_position, futurePosition), r + Config.MovementSafetyMargin)) {
		//_path.size() > 0 ? ActorRadius - MovementSafetyMargin : ActorRadius)) {
		result.allowed = false;
		Logger::log("Actor " + _name + " movement wasn't allowed.");
	}

	return result;
}

float Actor::getDistanceToGoal() const {
	if (_path.empty()) { return 0; }
	return (_position - _nextSafeGoal).length();
}

std::pair<Vector2, Vector2> Actor::getViewBorders() const {
	float angle = common::radians(Config.ActorVOCheckAngle);
	float from = _orientation - angle;
	float to = _orientation + angle;
	return std::make_pair(Vector2(cosf(from), sinf(from)), Vector2(cosf(to), sinf(to)));
}

std::vector<GameDynamicObject*> Actor::getObjectsInViewAngle() const {
	float angle = common::radians(Config.ActorVOCheckAngle);
	float from = _orientation - angle;
	float to = _orientation + angle;

	float r = getRadius();
	float safetyMargin = Config.MovementSafetyMargin;
	float voCheckRadius = Config.ActorVOCheckRadius;

	std::vector<GameDynamicObject*> result;
	for (GameDynamicObject* other : _nearbyObjects) {
		Vector2 otherPos = other->getPosition();
		if (
			(common::isAngleBetween(common::angleFromTo(_position, otherPos), from, to)
			|| common::distance(otherPos, Segment(_position, _position + Vector2(cosf(from), sinf(from)) * voCheckRadius)) <= r
			|| common::distance(otherPos, Segment(_position, _position + Vector2(cosf(to), sinf(to)) * voCheckRadius)) <= r)
			&& common::sqDist(otherPos, _position) <= common::sqr(voCheckRadius)
			|| common::sqDist(otherPos, _position) <= common::sqr(2 * r + common::EPSILON + safetyMargin)) {
			result.push_back(other);
		}
	}
	return result;
}

std::pair<Vector2, Vector2> getVOSides(const Vector2& point, const common::Circle& circle) {
	float angle = common::angleFromTo(point, circle.center);
	float openingAngle;
	if (!circle.contains(point)) {
		openingAngle = asinf(circle.radius / common::distance(circle.center, point));
	}
	else {
		openingAngle = common::PI_2_F;
	}
	openingAngle += Config.VOSideVelocityMargin;
	return std::make_pair(
		Vector2(cosf(angle - openingAngle), sinf(angle - openingAngle)),
		Vector2(cosf(angle + openingAngle), sinf(angle + openingAngle))
	);
}

std::vector<VelocityObstacle> Actor::getVelocityObstacles(const std::vector<GameDynamicObject*>& obstacles) const {
	std::vector<VelocityObstacle> result;
	result.reserve(obstacles.size());
	Vector2 pos = _position;
	float r = 2 * Config.ActorRadius;
	for (GameDynamicObject* other : obstacles) {
		//Vector2D apex = cder->is_static() ? pos : pos + (cder->predicted_velocity() + movable->velocity) / 2;
		Vector2 apex = pos;
		auto sides = getVOSides(pos, common::Circle(other->getPosition(), r));
		result.push_back({ apex, sides.first, sides.second, other });
	}
	return result;
}

std::vector<Actor::Candidate> Actor::computeCandidates(const std::vector<VelocityObstacle>& vo) const {

	std::vector<Candidate> result;
	Vector2 pos = _position;
	size_t n = vo.size();

	// Czy preferowany wektor ruchu jest dopuszczalny?
	bool accept = true;
	for (size_t i = 0; i < n && accept; ++i) {
		Vector2 vec = pos + _preferredVelocity - vo[i].apex;
		if (common::cross(vo[i].side1, vec) >= 0 && common::cross(vec, vo[i].side2) >= 0) {
			accept = false;
		}
	}

	float speed = getMaxSpeed();
	float voCheckRadius = Config.ActorVOCheckRadius;

	if (accept && _preferredVelocity.lengthSquared() > common::EPSILON) {
		Candidate c;
		c.velocity = _preferredVelocity;
		c.difference = 0;
		c.collisionFreeDistance = minDistanceWithoutCollision(_preferredVelocity, voCheckRadius);
		if (c.collisionFreeDistance > speed) {
			result.push_back(c);
		}
	}

	Vector2 currentVelocity = _velocity;

	// Sprawd� wektory ruchu wzd�u� kraw�dzi VO.
	for (size_t i = 0; i < n; ++i) {
		Vector2 v1 = common::adjustLength(vo[i].side1, speed), v2 = common::adjustLength(vo[i].side2, speed);
		Vector2 point1 = vo[i].apex + v1, point2 = vo[i].apex + v2;
		bool accept1 = true, accept2 = true;
		for (size_t j = 0; j < n && (accept1 || accept2); ++j) {
			if (i == j) { continue; }
			if (accept1) {
				Vector2 vec1 = point1 - vo[j].apex;
				if (common::cross(vo[j].side1, vec1) > 0 && common::cross(vec1, vo[j].side2) > 0) {
					accept1 = false;
				}
			}
			if (accept2) {
				Vector2 vec2 = point2 - vo[j].apex;
				if (common::cross(vo[j].side1, vec2) > 0 && common::cross(vec2, vo[j].side2) > 0) {
					accept2 = false;
				}
			}
		}
		if (accept1) {
			Candidate c;
			c.velocity = v1;
			c.difference = common::distance(v1, _preferredVelocity);
			c.collisionFreeDistance = minDistanceWithoutCollision(v1, voCheckRadius);
			result.push_back(c);
		}
		if (accept2) {
			Candidate c;
			c.velocity = v2;
			c.difference = common::distance(v2, _preferredVelocity);
			c.collisionFreeDistance = minDistanceWithoutCollision(v2, voCheckRadius);
			result.push_back(c);
		}
	}

	return result;
}

float Actor::minDistanceWithoutCollision(const Vector2& direction, float maxDistance) const {
	Vector2 dirVec = common::adjustLength(direction, maxDistance);
	Vector2 endPoint = _position + dirVec;
	/*Aabb aabb = Aabb(Vector2::min(_position, endPoint) - Vector2(ActorRadius, ActorRadius),
		abs(dirVec.x) + 2 * ActorRadius,
		abs(dirVec.y) + 2 * ActorRadius);*/
	Segment segment(_position, endPoint);
	float minDist = maxDistance;
	Segment movementSegment = Segment(_position, endPoint);
	float r = getRadius();
	
	for (auto c : getDynamicObjectsOnLine(segment)) {
		Vector2 otherPos = c->getPosition();
		float otherRadius = c->getRadius();
		if (c != this && c->isSolid() && common::distance(otherPos, movementSegment) <= r + otherRadius + common::EPSILON) {
			float dist = (otherPos - _position).length() - otherRadius - r;
			if (dist < minDist) { minDist = dist; }
		}
	}
	
	for (auto elem : getStaticObjectsOnLine(segment)) {
		float dist = elem->getDistanceTo(_position) - r;
		if (dist < minDist) { minDist = dist; }
	}

	return minDist;
}

Vector2 Actor::selectVelocity(const std::vector<Candidate>& candidates) const {
	if (candidates.empty()) { return Vector2(); }
	size_t min = 0;
	auto fval = [](Candidate c) -> float { 
		return c.difference + (Config.ActorVOCheckRadius - c.collisionFreeDistance) / Config.ActorVOCheckRadius; 
	};
	float minval = fval(candidates[min]);
	size_t n = candidates.size();
	for (size_t i = 1; i < n; ++i) {
		if (fval(candidates[i]) < minval) {
			min = i;
		}
	}
	return candidates[min].velocity;
}

std::vector<Wall*> Actor::getWallsNearGoal() const {
	std::vector<Wall*> result;
	if (!_path.empty()) {
		float dist = (1.1f * getRadius() + common::EPSILON) * common::SQRT_2_F;
		Vector2 point = _path.front();
		for (GameStaticObject* staticObj : Game::getInstance()->getMap()->getWalls()) {
			Wall* wall = (Wall*)staticObj;
			int wallId = wall->getId();
			if (!std::any_of(result.begin(), result.end(), [wallId](Wall* w)->bool { return w->getId() == wallId; })
				&& common::distance(point, wall->getSegment()) <= dist) {
				result.push_back(wall);
			}
		}
	}
	return result;
}

Vector2 Actor::getNextSafeGoal() const {
	if (!_path.empty()) {
		auto walls = getWallsNearGoal();
		if (walls.size() == 2) {
			Vector2 p11 = walls.at(0)->getFrom(),
				p12 = walls.at(0)->getTo(),
				p21 = walls.at(1)->getFrom(),
				p22 = walls.at(1)->getTo();

			bool validFlag = false;
			Vector2 commonPoint, otherPoint1, otherPoint2;
			if (common::sqDist(p11, p21) < common::EPSILON) {
				validFlag = true;
				commonPoint = p11;
				otherPoint1 = p12;
				otherPoint2 = p22;
			}
			else if (common::sqDist(p11, p22) < common::EPSILON) {
				validFlag = true;
				commonPoint = p11;
				otherPoint1 = p12;
				otherPoint2 = p21;
			}
			if (common::sqDist(p12, p21) < common::EPSILON) {
				validFlag = true;
				commonPoint = p12;
				otherPoint1 = p11;
				otherPoint2 = p22;
			}
			else if (common::sqDist(p12, p22) < common::EPSILON) {
				validFlag = true;
				commonPoint = p12;
				otherPoint1 = p11;
				otherPoint2 = p21;
			}

			if (validFlag) {
				Vector2 pos = _position;
				bool o1 = common::triangleOrientation(otherPoint1, commonPoint, pos)
					== common::triangleOrientation(otherPoint1, commonPoint, otherPoint2);
				bool o2 = common::triangleOrientation(otherPoint2, commonPoint, pos)
					== common::triangleOrientation(otherPoint2, commonPoint, otherPoint1);

				Vector2 w1 = (p12 - p11).normal();
				Vector2 w2 = (p22 - p21).normal();

				float temp = w1.x; w1.x = w1.y; w1.y = temp;
				temp = w2.x; w2.x = w2.y; w2.y = temp;

				Vector2 q11 = commonPoint + w1, q12 = commonPoint - w1;
				Vector2 q21 = commonPoint + w2, q22 = commonPoint - w2;

				Vector2 translation;

				if (o1 == o2) {
					translation = (common::sqDist(pos, q11) < common::sqDist(pos, q12) ? w1 : -w1)
						+ (common::sqDist(pos, q21) < common::sqDist(pos, q22) ? w2 : -w2);
				}
				else {
					if (o2) {
						translation = (common::sqDist(pos, q11) < common::sqDist(pos, q12) ? w1 : -w1)
							+ (common::sqDist(pos, q21) < common::sqDist(pos, q22) ? -w2 : w2);
					}
					else {
						translation = (common::sqDist(pos, q11) < common::sqDist(pos, q12) ? -w1 : w1)
							+ (common::sqDist(pos, q21) < common::sqDist(pos, q22) ? w2 : -w2);
					}
				}

				return commonPoint + translation.normal() * common::SQRT_2_F
					* (Config.ActorRadius + Config.MovementSafetyMargin + common::EPSILON);
			}
		}
		return _path.front();
	}
	return _position;
}

std::vector<GameDynamicObject*> Actor::getNearbyObjects() const {
	std::vector<GameDynamicObject*> result;
	Vector2 pos = getPosition();
	float maxDist = common::sqr(Config.ActorSightRadius);

	for (GameDynamicObject* entity : getDynamicObjectsInArea(_position, Config.ActorSightRadius)) {
		if (entity != this && common::sqDist(entity->getPosition(), pos) < maxDist
			&& getStaticObjectsOnLine(Segment(pos, entity->getPosition())).empty()) {
			result.push_back((Actor*)entity);
		}
	}	

	return result;
}

void Actor::updateSpotting() {
	// Podczas ruchu zaktualizuj zbi�r widzianych aktor�w
	if (hasPositionChanged()) {
		auto actorsNearby = getNearbyObjects();

		size_t notSpottedPreviously = _nearbyObjects.size();
		size_t notSpottedNow = actorsNearby.size();

		// Uaktualnij informacj� o s�siedztwie w przypadku aktor�w statycznych.
		for (GameDynamicObject* actor : _nearbyObjects) {
			
			// Je�eli aktor si� przemieszcza, to i tak sam wymieni ca�y wektor.
			if (actor->hasPositionChanged()) { continue; }

			// Je�eli si� nie porusza, to ustal czy aktor znikn�� czy pojawi� si� w pobli�u.
			size_t wasSpotted = common::indexOf(_nearbyObjects, actor);
			size_t isSpotted = common::indexOf(actorsNearby, actor);

			if (wasSpotted == notSpottedPreviously && isSpotted != notSpottedNow) {
				actor->spot(this);
			}
			else if (wasSpotted != notSpottedPreviously && isSpotted == notSpottedNow) {
				actor->unspot(this);
			}
		}

		_nearbyObjects = actorsNearby;
	}
}

bool Actor::isSpotting() const { return true; }

void Actor::spot(GameDynamicObject* entity) {
	_nearbyObjects.push_back(entity);
}

void Actor::unspot(GameDynamicObject* entity) {
	size_t n = _nearbyObjects.size();
	size_t k = common::indexOf(_nearbyObjects, entity);
	if (k != n) {
		if (k != n - 1) {
			_nearbyObjects[k] = _nearbyObjects.at(n - 1);
			_nearbyObjects.pop_back();
		}
	}
}
