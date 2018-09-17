#pragma once

#include <string>
#include <condition_variable>
#include <thread>
#include "Configuration.h"
#include "math/Math.h"
#include "engine/SegmentTree.h"
#include "engine/Navigation.h"
#include "engine/MissileManager.h"
#include "entities/Team.h"
#include "entities/Trigger.h"
#include "SDL.h"
#include "SDL_ttf.h"
#include "agents/Agent.h"
#include "agents/LuaEnvironment.h"

class Game {
public:
	Game();
	~Game();
	bool initialize(const char* title, int width, int height);
	bool isRunning();
	void handleEvents();
	void update();
	void render();
	void dispose();

	static Game* getInstance();
	
	GameTime getTime() const;
	GameMap* getMap() const;
	std::vector<Team*> getTeams() const;
	std::vector<Actor*> getActors() const;
	std::vector<Trigger*> getTriggers() const;
	MissileManager* getMissileManager() const;

private:
	static Game* _instance;

	GameTime _gameTime;
	std::mutex _updateMutex;
	std::condition_variable _updateHolder;

	bool _isRunning;
	SDL_Window* _window;
	SDL_Renderer* _renderer;
	
	LuaEnv* _luaEnv;
	GameMap* _gameMap;
	MissileManager* _missileManager;
	std::vector<Team*> _teams;

	PlayerAgent* _playerAgent;
	std::vector<Agent*> _agents;

	void initializeTeams(const String& filename);

	void drawTrigger(const Trigger& trigger) const;
	void drawActor(const Actor& actor) const;

	bool _areHealthBarsVisible = true;
	bool _isNavigationMeshVisible = false;
	bool _areAabbsVisible = false;
	bool _iscurrentPathVisible = false;
	bool _isUpdateEnabled = true;
	std::queue<Vector2> _path;

	int mousePosX;
	int mousePosY;
	
	void drawPoint(const Vector2& point, const SDL_Color& color) const;
	void drawSegment(const Segment& segment, const SDL_Color& color) const;
	void drawAabb(const Aabb& aabb, const SDL_Color& color) const;
	void drawString(const char* string, int x, int y, const SDL_Color& color) const; 
	void drawTexture(SDL_Texture* texture, const Vector2& position) const;
	void drawTexture(SDL_Texture* texture, const Vector2& position, float orientation, bool horizontalFlip) const;
	void fillAabb(const Aabb& aabb, const SDL_Color& color) const;
	void drawCircle(const Vector2& center, float radius, const SDL_Color& color) const;
	void fillCircle(const Vector2& center, float radius, const SDL_Color& color) const;
	void fillRing(const Vector2& center, float radius1, float radius2, const SDL_Color& color) const;

	friend class Agent;

};
