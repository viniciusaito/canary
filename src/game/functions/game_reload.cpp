/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2024 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "pch.hpp"

#include "game/functions/game_reload.hpp"

#include "config/configmanager.hpp"
#include "lua/creature/events.hpp"
#include "creatures/players/imbuements/imbuements.hpp"
#include "lua/scripts/lua_environment.hpp"
#include "lua/modules/modules.hpp"
#include "lua/scripts/scripts.hpp"
#include "game/zones/zone.hpp"

GameReload::GameReload() = default;
GameReload::~GameReload() = default;

bool GameReload::init(Reload_t reloadTypes) const {
	switch (reloadTypes) {
		case Reload_t::RELOAD_TYPE_ALL:
			return reloadAll();
		case Reload_t::RELOAD_TYPE_CHAT:
			return reloadChat();
		case Reload_t::RELOAD_TYPE_CONFIG:
			return reloadConfig();
		case Reload_t::RELOAD_TYPE_EVENTS:
			return reloadEvents();
		case Reload_t::RELOAD_TYPE_CORE:
			return reloadCore();
		case Reload_t::RELOAD_TYPE_IMBUEMENTS:
			return reloadImbuements();
		case Reload_t::RELOAD_TYPE_ITEMS:
			return reloadItems();
		case Reload_t::RELOAD_TYPE_MODULES:
			return reloadModules();
		case Reload_t::RELOAD_TYPE_MONSTERS:
			return reloadMonsters();
		case Reload_t::RELOAD_TYPE_MOUNTS:
			return reloadMounts();
		case Reload_t::RELOAD_TYPE_NPCS:
			return reloadNpcs();
		case Reload_t::RELOAD_TYPE_RAIDS:
			return reloadRaids();
		case Reload_t::RELOAD_TYPE_SCRIPTS:
			return reloadScripts();
		case Reload_t::RELOAD_TYPE_GROUPS:
			return reloadGroups();
		case Reload_t::RELOAD_TYPE_VOCATIONS:
			return reloadVocations();
		default:
			return false;
	}
}

uint8_t GameReload::getReloadNumber(Reload_t reloadTypes) const {
	return magic_enum::enum_integer(reloadTypes);
}

// Helper function for logging reload status
void logReloadStatus(const std::string &name, bool result) {
	if (result) {
		g_logger().info("Reloaded: {}", name);
	} else {
		g_logger().error("Failed to reload: {}", name);
	}
}

/*
 * From here down have the private members functions
 * These should only be used within the class itself
 * If it is necessary to call elsewhere, seriously think about creating a function that calls this
 * Changing this to public may cause some unexpected behavior or bug
 */
bool GameReload::reloadAll() const {
	std::vector<bool> reloadResults;
	reloadResults.reserve(magic_enum::enum_count<Reload_t>());

	for (auto value : magic_enum::enum_values<Reload_t>()) {
		const auto name = magic_enum::enum_name(value);
		g_logger().info("Reloading: {}", name);
		if (value != Reload_t::RELOAD_TYPE_ALL) {
			reloadResults.push_back(init(value));
		}
	}

	return std::ranges::any_of(reloadResults, [](bool result) { return result; });
}

bool GameReload::reloadChat() const {
	const bool result = g_chat().load();
	logReloadStatus("Chat", result);
	return result;
}

bool GameReload::reloadConfig() const {
	const bool result = g_configManager().reload();
	logReloadStatus("Config", result);
	return result;
}

bool GameReload::reloadEvents() const {
	const bool result = g_events().loadFromXml();
	logReloadStatus("Events", result);
	return result;
}

bool GameReload::reloadCore() const {
	const auto &coreFolder = g_configManager().getString(CORE_DIRECTORY, __FUNCTION__);
	const bool coreLoaded = g_luaEnvironment().loadFile(coreFolder + "/core.lua", "core.lua") == 0;

	if (coreLoaded) {
		const auto &datapackFolder = g_configManager().getString(CORE_DIRECTORY, __FUNCTION__);
		const bool scriptsLoaded = g_scripts().loadScripts(coreFolder + "/scripts/lib", true, false);
		if (scriptsLoaded) {
			return true;
		}
	}

	logReloadStatus("Core", false);
	return false;
}

bool GameReload::reloadImbuements() const {
	const bool result = g_imbuements().reload();
	logReloadStatus("Imbuements", result);
	return result;
}

bool GameReload::reloadItems() const {
	const bool result = Item::items.reload();
	logReloadStatus("Items", result);
	return result;
}

bool GameReload::reloadModules() const {
	const bool result = g_modules().reload();
	logReloadStatus("Modules", result);
	return result;
}

bool GameReload::reloadMonsters() const {
	g_monsters().clear();
	const auto &datapackFolder = g_configManager().getString(DATA_DIRECTORY, __FUNCTION__);
	const auto &coreFolder = g_configManager().getString(CORE_DIRECTORY, __FUNCTION__);

	const bool scriptsLoaded = g_scripts().loadScripts(coreFolder + "/scripts/lib", true, false);
	const bool monsterScriptsLoaded = g_scripts().loadScripts(datapackFolder + "/monster", false, true);

	if (scriptsLoaded && monsterScriptsLoaded) {
		logReloadStatus("Monsters", true);
		return true;
	} else {
		logReloadStatus("Monsters", false);
		return false;
	}
}

bool GameReload::reloadMounts() const {
	const bool result = g_game().mounts.reload();
	logReloadStatus("Mounts", result);
	return result;
}

bool GameReload::reloadNpcs() const {
	const bool result = g_npcs().reload();
	logReloadStatus("NPCs", result);
	return result;
}

bool GameReload::reloadRaids() const {
	const bool result = g_game().raids.reload() && g_game().raids.startup();
	logReloadStatus("Raids", result);
	return result;
}

bool GameReload::reloadScripts() const {
	g_scripts().clearAllScripts();
	Zone::clearZones();

	const auto &datapackFolder = g_configManager().getString(DATA_DIRECTORY, __FUNCTION__);
	const auto &coreFolder = g_configManager().getString(CORE_DIRECTORY, __FUNCTION__);

	g_scripts().loadScripts(coreFolder + "/scripts/lib", true, false);
	g_scripts().loadScripts(datapackFolder + "/scripts", false, true);
	g_scripts().loadScripts(coreFolder + "/scripts", false, true);

	// It should come last, after everything else has been cleaned up.
	reloadMonsters();
	reloadNpcs();
	logReloadStatus("Scripts", true);
	return true;
}

bool GameReload::reloadGroups() const {
	const bool result = g_game().groups.reload();
	logReloadStatus("Groups", result);
	return result;
}

bool GameReload::reloadVocations() const {
	const bool result = g_vocations().reload();
	logReloadStatus("Vocations", result);
	return result;
}
