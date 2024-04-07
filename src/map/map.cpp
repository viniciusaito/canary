/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2024 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "pch.hpp"

#include "map.hpp"
#include "utils/astarnodes.hpp"

#include "creatures/monsters/monster.hpp"
#include "game/game.hpp"
#include "game/zones/zone.hpp"
#include "io/iomap.hpp"
#include "io/iomapserialize.hpp"
#include "game/scheduling/dispatcher.hpp"
#include "map/spectators.hpp"

void Map::load(const std::string &identifier, const Position &pos) {
	try {
		path = identifier;
		IOMap::loadMap(this, pos);
	} catch (const std::exception &e) {
		throw IOMapException(fmt::format(
			"\n[Map::load] - The map in folder {} is missing or corrupted"
			"\n            - {}",
			identifier, e.what()
		));
	}
}

void Map::loadMap(const std::string &identifier, bool mainMap /*= false*/, bool loadHouses /*= false*/, bool loadMonsters /*= false*/, bool loadNpcs /*= false*/, bool loadZones /*= false*/, const Position &pos /*= Position()*/) {
	// Only download map if is loading the main map and it is not already downloaded
	if (mainMap && g_configManager().getBoolean(TOGGLE_DOWNLOAD_MAP, __FUNCTION__) && !std::filesystem::exists(identifier)) {
		const auto mapDownloadUrl = g_configManager().getString(MAP_DOWNLOAD_URL, __FUNCTION__);
		if (mapDownloadUrl.empty()) {
			g_logger().warn("Map download URL in config.lua is empty, download disabled");
		}

		if (CURL* curl = curl_easy_init(); curl && !mapDownloadUrl.empty()) {
			g_logger().info("Downloading " + g_configManager().getString(MAP_NAME, __FUNCTION__) + ".otbm to world folder");
			FILE* otbm = fopen(identifier.c_str(), "wb");
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl, CURLOPT_URL, mapDownloadUrl.c_str());
			curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, otbm);
			curl_easy_perform(curl);
			curl_easy_cleanup(curl);
			fclose(otbm);
		}
	}

	// Load the map
	load(identifier, pos);

	// Only create items from lua functions if is loading main map
	// It needs to be after the load map to ensure the map already exists before creating the items
	if (mainMap) {
		// Create items from lua scripts per position
		// Example: ActionFunctions::luaActionPosition
		g_game().createLuaItemsOnMap();
	}

	if (loadMonsters) {
		IOMap::loadMonsters(this);
	}

	if (loadHouses) {
		IOMap::loadHouses(this);

		/**
		 * Only load houses items if map custom load is disabled
		 * If map custom is enabled, then it is load in loadMapCustom function
		 * NOTE: This will ensure that the information is not duplicated
		 */
		if (!g_configManager().getBoolean(TOGGLE_MAP_CUSTOM, __FUNCTION__)) {
			IOMapSerialize::loadHouseInfo();
			IOMapSerialize::loadHouseItems(this);
		}
	}

	if (loadNpcs) {
		IOMap::loadNpcs(this);
	}

	if (loadZones) {
		IOMap::loadZones(this);
	}

	// Files need to be cleaned up if custom map is enabled to open, or will try to load main map files
	if (g_configManager().getBoolean(TOGGLE_MAP_CUSTOM, __FUNCTION__)) {
		monsterfile.clear();
		housefile.clear();
		npcfile.clear();
	}
}

void Map::loadMapCustom(const std::string &mapName, bool loadHouses, bool loadMonsters, bool loadNpcs, bool loadZones, int customMapIndex) {
	// Load the map
	load(g_configManager().getString(DATA_DIRECTORY, __FUNCTION__) + "/world/custom/" + mapName + ".otbm");

	if (loadMonsters && !IOMap::loadMonstersCustom(this, mapName, customMapIndex)) {
		g_logger().warn("Failed to load monster custom data");
	}

	if (loadHouses && !IOMap::loadHousesCustom(this, mapName, customMapIndex)) {
		g_logger().warn("Failed to load house custom data");
	}

	if (loadNpcs && !IOMap::loadNpcsCustom(this, mapName, customMapIndex)) {
		g_logger().warn("Failed to load npc custom spawn data");
	}

	if (loadZones && !IOMap::loadZonesCustom(this, mapName, customMapIndex)) {
		g_logger().warn("Failed to load zones custom data");
	}

	// Files need to be cleaned up or will try to load previous map files again
	monsterfile.clear();
	housefile.clear();
	npcfile.clear();
}

void Map::loadHouseInfo() {
	IOMapSerialize::loadHouseInfo();
	IOMapSerialize::loadHouseItems(this);
}

bool Map::save() {
	bool saved = false;
	for (uint32_t tries = 0; tries < 6; tries++) {
		if (IOMapSerialize::saveHouseInfo()) {
			saved = true;
		}
		if (saved && IOMapSerialize::saveHouseItems()) {
			return true;
		}
	}
	return false;
}

std::shared_ptr<Tile> Map::getOrCreateTile(uint16_t x, uint16_t y, uint8_t z, bool isDynamic) {
	auto tile = getTile(x, y, z);
	if (!tile) {
		if (isDynamic) {
			tile = std::make_shared<DynamicTile>(x, y, z);
		} else {
			tile = std::make_shared<StaticTile>(x, y, z);
		}

		setTile(x, y, z, tile);
	}

	return tile;
}

std::shared_ptr<Tile> Map::getLoadedTile(uint16_t x, uint16_t y, uint8_t z) {
	if (z >= MAP_MAX_LAYERS) {
		return nullptr;
	}

	const auto leaf = getQTNode(x, y);
	if (!leaf) {
		return nullptr;
	}

	const auto &floor = leaf->getFloor(z);
	if (!floor) {
		return nullptr;
	}

	const auto tile = floor->getTile(x, y);
	return tile;
}

std::shared_ptr<Tile> Map::getTile(uint16_t x, uint16_t y, uint8_t z) {
	if (z >= MAP_MAX_LAYERS) {
		return nullptr;
	}

	const auto leaf = getQTNode(x, y);
	if (!leaf) {
		return nullptr;
	}

	const auto &floor = leaf->getFloor(z);
	if (!floor) {
		return nullptr;
	}

	const auto tile = floor->getTile(x, y);
	return tile ? tile : getOrCreateTileFromCache(floor, x, y);
}

void Map::refreshZones(uint16_t x, uint16_t y, uint8_t z) {
	const auto tile = getLoadedTile(x, y, z);
	if (!tile) {
		return;
	}

	tile->clearZones();
	const auto &zones = Zone::getZones(tile->getPosition());
	for (const auto &zone : zones) {
		tile->addZone(zone);
	}
}

void Map::setTile(uint16_t x, uint16_t y, uint8_t z, std::shared_ptr<Tile> newTile) {
	if (z >= MAP_MAX_LAYERS) {
		g_logger().error("Attempt to set tile on invalid coordinate: {}", Position(x, y, z).toString());
		return;
	}

	if (const auto leaf = getQTNode(x, y)) {
		leaf->createFloor(z)->setTile(x, y, newTile);
	} else {
		root.getBestLeaf(x, y, 15)->createFloor(z)->setTile(x, y, newTile);
	}
}

bool Map::placeCreature(const Position &centerPos, std::shared_ptr<Creature> creature, bool extendedPos /* = false*/, bool forceLogin /* = false*/) {
	auto monster = creature->getMonster();
	if (monster) {
		monster->ignoreFieldDamage = true;
	}

	bool foundTile;
	bool placeInPZ;

	std::shared_ptr<Tile> tile = getTile(centerPos.x, centerPos.y, centerPos.z);
	if (tile) {
		placeInPZ = tile->hasFlag(TILESTATE_PROTECTIONZONE);
		ReturnValue ret = tile->queryAdd(0, creature, 1, FLAG_IGNOREBLOCKITEM | FLAG_IGNOREFIELDDAMAGE);
		foundTile = forceLogin || ret == RETURNVALUE_NOERROR || ret == RETURNVALUE_PLAYERISNOTINVITED;
		if (monster) {
			monster->ignoreFieldDamage = false;
		}
	} else {
		placeInPZ = false;
		foundTile = false;
	}

	if (!foundTile) {
		static std::vector<std::pair<int32_t, int32_t>> extendedRelList {
			{ 0, -2 },
			{ -1, -1 },
			{ 0, -1 },
			{ 1, -1 },
			{ -2, 0 },
			{ -1, 0 },
			{ 1, 0 },
			{ 2, 0 },
			{ -1, 1 },
			{ 0, 1 },
			{ 1, 1 },
			{ 0, 2 }
		};

		static std::vector<std::pair<int32_t, int32_t>> normalRelList {
			{ -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 }
		};

		std::vector<std::pair<int32_t, int32_t>> &relList = (extendedPos ? extendedRelList : normalRelList);

		if (extendedPos) {
			std::shuffle(relList.begin(), relList.begin() + 4, getRandomGenerator());
			std::shuffle(relList.begin() + 4, relList.end(), getRandomGenerator());
		} else {
			std::shuffle(relList.begin(), relList.end(), getRandomGenerator());
		}

		for (const auto &it : relList) {
			Position tryPos(centerPos.x + it.first, centerPos.y + it.second, centerPos.z);

			tile = getTile(tryPos.x, tryPos.y, tryPos.z);
			if (!tile || (placeInPZ && !tile->hasFlag(TILESTATE_PROTECTIONZONE))) {
				continue;
			}

			// Will never add the creature inside a teleport, avoiding infinite loop bug
			if (tile->hasFlag(TILESTATE_TELEPORT)) {
				continue;
			}

			if (monster) {
				monster->ignoreFieldDamage = true;
			}

			if (tile->queryAdd(0, creature, 1, FLAG_IGNOREBLOCKITEM | FLAG_IGNOREFIELDDAMAGE) == RETURNVALUE_NOERROR) {
				if (!extendedPos || isSightClear(centerPos, tryPos, false)) {
					foundTile = true;
					break;
				}
			}
		}

		if (!foundTile) {
			return false;
		} else {
			if (monster) {
				monster->ignoreFieldDamage = false;
			}
		}
	}

	int32_t index = 0;
	uint32_t flags = 0;
	std::shared_ptr<Item> toItem = nullptr;

	auto toCylinder = tile->queryDestination(index, creature, &toItem, flags);
	toCylinder->internalAddThing(creature);

	const Position &dest = toCylinder->getPosition();
	getQTNode(dest.x, dest.y)->addCreature(creature);
	return true;
}

void Map::moveCreature(const std::shared_ptr<Creature> &creature, const std::shared_ptr<Tile> &newTile, bool forceTeleport /* = false*/) {
	auto oldTile = creature->getTile();

	Position oldPos = oldTile->getPosition();
	Position newPos = newTile->getPosition();

	const auto &fromZones = oldTile->getZones();
	const auto &toZones = newTile->getZones();
	if (auto ret = g_game().beforeCreatureZoneChange(creature, fromZones, toZones); ret != RETURNVALUE_NOERROR) {
		return;
	}

	bool teleport = forceTeleport || !newTile->getGround() || !Position::areInRange<1, 1, 0>(oldPos, newPos);

	auto spectators = Spectators()
						  .find<Creature>(oldPos, true)
						  .find<Creature>(newPos, true);

	auto playersSpectators = spectators.filter<Player>();

	std::vector<int32_t> oldStackPosVector;
	for (const auto &spec : playersSpectators) {
		if (spec->canSeeCreature(creature)) {
			oldStackPosVector.push_back(oldTile->getClientIndexOfCreature(spec->getPlayer(), creature));
		} else {
			oldStackPosVector.push_back(-1);
		}
	}

	// remove the creature
	oldTile->removeThing(creature, 0);

	auto leaf = getQTNode(oldPos.x, oldPos.y);
	auto new_leaf = getQTNode(newPos.x, newPos.y);

	// Switch the node ownership
	if (leaf != new_leaf) {
		leaf->removeCreature(creature);
		new_leaf->addCreature(creature);
	}

	// add the creature
	newTile->addThing(creature);

	if (!teleport) {
		if (oldPos.y > newPos.y) {
			creature->setDirection(DIRECTION_NORTH);
		} else if (oldPos.y < newPos.y) {
			creature->setDirection(DIRECTION_SOUTH);
		}

		if (oldPos.x < newPos.x) {
			creature->setDirection(DIRECTION_EAST);
		} else if (oldPos.x > newPos.x) {
			creature->setDirection(DIRECTION_WEST);
		}
	}

	// send to client
	size_t i = 0;
	for (const auto &spectator : playersSpectators) {
		// Use the correct stackpos
		int32_t stackpos = oldStackPosVector[i++];
		if (stackpos != -1) {
			const auto &player = spectator->getPlayer();
			player->sendCreatureMove(creature, newPos, newTile->getStackposOfCreature(player, creature), oldPos, stackpos, teleport);
		}
	}

	// event method
	for (const auto &spectator : spectators) {
		spectator->onCreatureMove(creature, newTile, newPos, oldTile, oldPos, teleport);
	}

	oldTile->postRemoveNotification(creature, newTile, 0);
	newTile->postAddNotification(creature, oldTile, 0);
	g_game().afterCreatureZoneChange(creature, fromZones, toZones);
}

bool Map::canThrowObjectTo(const Position &fromPos, const Position &toPos, bool checkLineOfSight /*= true*/, int32_t rangex /*= MAP_MAX_CLIENT_VIEW_PORT_X*/, int32_t rangey /*= MAP_MAX_CLIENT_VIEW_PORT_Y*/) {
	// z checks
	// underground 8->15
	// ground level and above 7->0
	if ((fromPos.z >= 8 && toPos.z <= MAP_INIT_SURFACE_LAYER) || (toPos.z >= MAP_INIT_SURFACE_LAYER + 1 && fromPos.z <= MAP_INIT_SURFACE_LAYER)) {
		return false;
	}

	int32_t deltaz = Position::getDistanceZ(fromPos, toPos);
	if (deltaz > MAP_LAYER_VIEW_LIMIT) {
		return false;
	}

	if ((Position::getDistanceX(fromPos, toPos) - deltaz) > rangex) {
		return false;
	}

	// distance checks
	if ((Position::getDistanceY(fromPos, toPos) - deltaz) > rangey) {
		return false;
	}

	if (!checkLineOfSight) {
		return true;
	}
	return isSightClear(fromPos, toPos, false);
}

bool Map::checkSightLine(const Position &fromPos, const Position &toPos) {
	if (fromPos == toPos) {
		return true;
	}

	Position start(fromPos.z > toPos.z ? toPos : fromPos);
	Position destination(fromPos.z > toPos.z ? fromPos : toPos);

	const int8_t mx = start.x < destination.x ? 1 : start.x == destination.x ? 0
																			 : -1;
	const int8_t my = start.y < destination.y ? 1 : start.y == destination.y ? 0
																			 : -1;

	int32_t A = Position::getOffsetY(destination, start);
	int32_t B = Position::getOffsetX(start, destination);
	int32_t C = -(A * destination.x + B * destination.y);

	while (start.x != destination.x || start.y != destination.y) {
		int32_t move_hor = std::abs(A * (start.x + mx) + B * (start.y) + C);
		int32_t move_ver = std::abs(A * (start.x) + B * (start.y + my) + C);
		int32_t move_cross = std::abs(A * (start.x + mx) + B * (start.y + my) + C);

		if (start.y != destination.y && (start.x == destination.x || move_hor > move_ver || move_hor > move_cross)) {
			start.y += my;
		}

		if (start.x != destination.x && (start.y == destination.y || move_ver > move_hor || move_ver > move_cross)) {
			start.x += mx;
		}

		const std::shared_ptr<Tile> tile = getTile(start.x, start.y, start.z);
		if (tile && tile->hasProperty(CONST_PROP_BLOCKPROJECTILE)) {
			return false;
		}
	}

	// now we need to perform a jump between floors to see if everything is clear (literally)
	while (start.z != destination.z) {
		const std::shared_ptr<Tile> tile = getTile(start.x, start.y, start.z);
		if (tile && tile->getThingCount() > 0) {
			return false;
		}

		start.z++;
	}

	return true;
}

bool Map::isSightClear(const Position &fromPos, const Position &toPos, bool floorCheck) {
	if (floorCheck && fromPos.z != toPos.z) {
		return false;
	}

	// Cast two converging rays and see if either yields a result.
	return checkSightLine(fromPos, toPos) || checkSightLine(toPos, fromPos);
}

std::shared_ptr<Tile> Map::canWalkTo(const std::shared_ptr<Creature> &creature, const Position &pos) {
	if (!creature || creature->isRemoved()) {
		return nullptr;
	}

	const int32_t walkCache = creature->getWalkCache(pos);

	if (walkCache == 0) {
		return nullptr;
	}

	if (walkCache == 1) {
		return getTile(pos.x, pos.y, pos.z);
	}

	// used for non-cached tiles
	const auto &tile = getTile(pos.x, pos.y, pos.z);
	if (creature->getTile() != tile) {
		if (!tile || tile->queryAdd(0, creature, 1, FLAG_PATHFINDING | FLAG_IGNOREFIELDDAMAGE) != RETURNVALUE_NOERROR) {
			return nullptr;
		}
	}

	return tile;
}

bool Map::getPathMatching(const std::shared_ptr<Creature> &creature, stdext::arraylist<Direction> &dirList, const FrozenPathingConditionCall &pathCondition, const FindPathParams &fpp) {
	return getPathMatching(creature, creature->getPosition(), dirList, pathCondition, fpp);
}

bool Map::getPathMatching(const std::shared_ptr<Creature> &creature, const Position &startPos, stdext::arraylist<Direction> &dirList, const FrozenPathingConditionCall &pathCondition, const FindPathParams &fpp) {
	static int_fast32_t allNeighbors[8][2] = {
		{ -1, 0 }, { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 }
	};

	static int_fast32_t dirNeighbors[8][5][2] = {
		{ { -1, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { -1, 1 } },
		{ { -1, 0 }, { 0, 1 }, { 0, -1 }, { -1, -1 }, { -1, 1 } },
		{ { -1, 0 }, { 1, 0 }, { 0, -1 }, { -1, -1 }, { 1, -1 } },
		{ { 0, 1 }, { 1, 0 }, { 0, -1 }, { 1, -1 }, { 1, 1 } },
		{ { 1, 0 }, { 0, -1 }, { -1, -1 }, { 1, -1 }, { 1, 1 } },
		{ { -1, 0 }, { 0, -1 }, { -1, -1 }, { 1, -1 }, { -1, 1 } },
		{ { 0, 1 }, { 1, 0 }, { 1, -1 }, { 1, 1 }, { -1, 1 } },
		{ { -1, 0 }, { 0, 1 }, { -1, -1 }, { 1, 1 }, { -1, 1 } }
	};

	Position pos = startPos;
	Position endPos;

	AStarNodes nodes(pos.x, pos.y);

	int32_t bestMatch = 0;

	AStarNode* found = nullptr;
	while (fpp.maxSearchDist != 0 || nodes.getClosedNodes() < 100) {
		AStarNode* n = nodes.getBestNode();
		if (!n) {
			if (found) {
				break;
			}
			return false;
		}

		const int_fast32_t x = n->x;
		const int_fast32_t y = n->y;
		pos.x = x;
		pos.y = y;
		if (pathCondition(startPos, pos, fpp, bestMatch)) {
			found = n;
			endPos = pos;
			if (bestMatch == 0) {
				break;
			}
		}

		uint_fast32_t dirCount;
		int_fast32_t* neighbors;
		if (n->parent) {
			const int_fast32_t offset_x = n->parent->x - x;
			const int_fast32_t offset_y = n->parent->y - y;
			if (offset_y == 0) {
				if (offset_x == -1) {
					neighbors = *dirNeighbors[DIRECTION_WEST];
				} else {
					neighbors = *dirNeighbors[DIRECTION_EAST];
				}
			} else if (!fpp.allowDiagonal || offset_x == 0) {
				if (offset_y == -1) {
					neighbors = *dirNeighbors[DIRECTION_NORTH];
				} else {
					neighbors = *dirNeighbors[DIRECTION_SOUTH];
				}
			} else if (offset_y == -1) {
				if (offset_x == -1) {
					neighbors = *dirNeighbors[DIRECTION_NORTHWEST];
				} else {
					neighbors = *dirNeighbors[DIRECTION_NORTHEAST];
				}
			} else if (offset_x == -1) {
				neighbors = *dirNeighbors[DIRECTION_SOUTHWEST];
			} else {
				neighbors = *dirNeighbors[DIRECTION_SOUTHEAST];
			}
			dirCount = fpp.allowDiagonal ? 5 : 3;
		} else {
			dirCount = 8;
			neighbors = *allNeighbors;
		}

		const int_fast32_t f = n->f;
		for (uint_fast32_t i = 0; i < dirCount; ++i) {
			pos.x = x + *neighbors++;
			pos.y = y + *neighbors++;

			if (fpp.maxSearchDist != 0 && (Position::getDistanceX(startPos, pos) > fpp.maxSearchDist || Position::getDistanceY(startPos, pos) > fpp.maxSearchDist)) {
				continue;
			}

			if (fpp.keepDistance && !pathCondition.isInRange(startPos, pos, fpp)) {
				continue;
			}

			AStarNode* neighborNode = nodes.getNodeByPosition(pos.x, pos.y);

			const bool withoutCreature = creature == nullptr;
			const auto &tile = neighborNode || withoutCreature ? getTile(pos.x, pos.y, pos.z) : canWalkTo(creature, pos);

			if (!tile || (!neighborNode && withoutCreature && tile->hasFlag(TILESTATE_BLOCKSOLID))) {
				continue;
			}

			// The cost (g) for this neighbor
			const int_fast32_t cost = AStarNodes::getMapWalkCost(n, pos, withoutCreature);
			const int_fast32_t extraCost = AStarNodes::getTileWalkCost(creature, tile);
			const int_fast32_t newf = f + cost + extraCost;

			if (neighborNode) {
				if (neighborNode->f <= newf) {
					// The node on the closed/open list is cheaper than this one
					continue;
				}

				neighborNode->f = newf;
				neighborNode->parent = n;
				nodes.openNode(neighborNode);
			} else {
				// Does not exist in the open/closed list, create a std::make_shared<node>
				neighborNode = nodes.createOpenNode(n, pos.x, pos.y, newf);
				if (!neighborNode) {
					if (found) {
						break;
					}
					return false;
				}
			}
		}

		nodes.closeNode(n);
	}

	if (!found) {
		return false;
	}

	int_fast32_t prevx = endPos.x;
	int_fast32_t prevy = endPos.y;

	found = found->parent;
	while (found) {
		pos.x = found->x;
		pos.y = found->y;

		const int_fast32_t dx = pos.getX() - prevx;
		const int_fast32_t dy = pos.getY() - prevy;

		prevx = pos.x;
		prevy = pos.y;

		if (dx == 1 && dy == 1) {
			dirList.push_front(DIRECTION_NORTHWEST);
		} else if (dx == -1 && dy == 1) {
			dirList.push_front(DIRECTION_NORTHEAST);
		} else if (dx == 1 && dy == -1) {
			dirList.push_front(DIRECTION_SOUTHWEST);
		} else if (dx == -1 && dy == -1) {
			dirList.push_front(DIRECTION_SOUTHEAST);
		} else if (dx == 1) {
			dirList.push_front(DIRECTION_WEST);
		} else if (dx == -1) {
			dirList.push_front(DIRECTION_EAST);
		} else if (dy == 1) {
			dirList.push_front(DIRECTION_NORTH);
		} else if (dy == -1) {
			dirList.push_front(DIRECTION_SOUTH);
		}

		found = found->parent;
	}
	return true;
}

uint32_t Map::clean() {
	uint64_t start = OTSYS_TIME();
	size_t tiles = 0;

	if (g_game().getGameState() == GAME_STATE_NORMAL) {
		g_game().setGameState(GAME_STATE_MAINTAIN);
	}

	std::vector<std::shared_ptr<Item>> toRemove;
	for (const auto &tile : g_game().getTilesToClean()) {
		if (!tile) {
			continue;
		}
		if (const auto items = tile->getItemList()) {
			++tiles;
			for (const auto &item : *items) {
				if (item->isCleanable()) {
					toRemove.emplace_back(item);
				}
			}
		}
	}

	for (const auto &item : toRemove) {
		g_game().internalRemoveItem(item, -1);
	}

	size_t count = toRemove.size();
	g_game().clearTilesToClean();

	if (g_game().getGameState() == GAME_STATE_MAINTAIN) {
		g_game().setGameState(GAME_STATE_NORMAL);
	}

	uint64_t end = OTSYS_TIME();
	g_logger().info("CLEAN: Removed {} item{} from {} tile{} in {} seconds", count, (count != 1 ? "s" : ""), tiles, (tiles != 1 ? "s" : ""), (end - start) / (1000.f));
	return count;
}
