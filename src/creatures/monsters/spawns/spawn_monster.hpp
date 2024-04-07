/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2024 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

#include "items/tile.hpp"
#include "game/movement/position.hpp"

class Monster;
class MonsterType;

struct spawnBlock_t {
	Position pos;
	std::unordered_map<std::shared_ptr<MonsterType>, uint32_t> monsterTypes;
	int64_t lastSpawn;
	uint32_t interval;
	Direction direction;

	std::shared_ptr<MonsterType> getMonsterType() const;
	bool hasBoss() const;
};

class SpawnMonster {
public:
	SpawnMonster(Position initPos, int32_t initRadius) :
		centerPos(initPos), radius(initRadius) { }
	~SpawnMonster();

	// non-copyable
	SpawnMonster(const SpawnMonster &) = delete;
	SpawnMonster &operator=(const SpawnMonster &) = delete;

	bool addMonster(const std::string &name, const Position &pos, Direction dir, uint32_t interval, uint32_t weight = 1);
	void removeMonster(std::shared_ptr<Monster> monster);
	void removeMonsters();

	uint32_t getInterval() const {
		return interval;
	}
	void startup(bool delayed = false);

	void startSpawnMonsterCheck();
	void stopEvent();

	bool isInSpawnMonsterZone(const Position &pos);
	void cleanup();

	const Position &getCenterPos() const {
		return centerPos;
	}

	void setMonsterVariant(const std::string &variant);

private:
	// map of the spawned creatures
	std::map<uint32_t, std::shared_ptr<Monster>> spawnedMonsterMap;

	// map of creatures in the spawn
	std::map<uint32_t, spawnBlock_t> spawnMonsterMap;

	Position centerPos;
	int32_t radius;

	uint32_t interval = 30000;
	uint32_t checkSpawnMonsterEvent = 0;

	static bool findPlayer(const Position &pos);
	bool spawnMonster(uint32_t spawnMonsterId, spawnBlock_t &sb, std::shared_ptr<MonsterType> monsterType, bool startup = false);
	void checkSpawnMonster();
	void scheduleSpawn(uint32_t spawnMonsterId, spawnBlock_t &sb, std::shared_ptr<MonsterType> monsterType, uint16_t interval, bool startup = false);
};

class SpawnsMonster {
public:
	static bool isInZone(const Position &centerPos, int32_t radius, const Position &pos);

	bool loadFromXML(const std::string &filemonstername);
	void startup();
	void clear();
	SpawnMonster &addSpawnMonster(const Position &pos, int32_t radius) {
		spawnMonsterList.emplace_front(pos, radius);
		return spawnMonsterList.front();
	}

	bool isStarted() const {
		return started;
	}
	bool isLoaded() const {
		return loaded;
	}
	std::forward_list<SpawnMonster> &getspawnMonsterList() {
		return spawnMonsterList;
	}

private:
	std::forward_list<SpawnMonster> spawnMonsterList;
	std::string filemonstername;
	bool loaded = false;
	bool started = false;
};

static constexpr int32_t NONBLOCKABLE_SPAWN_MONSTER_INTERVAL = 1400;
