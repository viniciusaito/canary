/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2022 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "pch.hpp"

#include "protocolspectator.hpp"

#include "server/network/protocol/protocolgame.hpp"

#include "creatures/players/player.hpp"
#include "creatures/interactions/chat.hpp"
#include "game/game.hpp"

#include "database/database.hpp"
#include "utils/tools.hpp"
#include "config/configmanager.hpp"
#include "map/map.hpp"

using LiveCastsMap = std::unordered_map<std::shared_ptr<Player>, std::shared_ptr<ProtocolGame>>;
const LiveCastsMap &ProtocolSpectator::getLiveCasts() const {
	return m_liveCasts;
}

void ProtocolSpectator::removeCaster() {
	if (m_owner) {
		m_owner->removeCaster();
	}
}

bool ProtocolSpectator::check(const std::string& _password)
{
	if(password.empty())
		return true;

	std::string t = _password;
	trimString(t);

	return t == password;
}

void ProtocolSpectator::kick(StringVector list)
{
	for (const auto& it : list) {
		for (const auto& sit : spectators) {
			if (!sit.first->m_spy && asLowerCaseString(sit.second.first) == it) {
				sit.first->disconnect();
			}
		}
	}
}

void ProtocolSpectator::ban(StringVector _bans)
{
	StringVector::const_iterator it;
	for (auto bit = bans.begin(); bit != bans.end(); ) {
		it = std::find(_bans.begin(), _bans.end(), bit->first);
		if (it == _bans.end()) {
			bans.erase(bit++);
		} else {
			++bit;
		}
	}

	for (it = _bans.begin(); it != _bans.end(); ++it) {
		for (const auto& sit : spectators) {
			if (sit.first->m_spy || asLowerCaseString(sit.second.first) != *it) {
				continue;
			}

			bans[*it] = sit.first->getIP();
			sit.first->disconnect();
		}
	}
}

void ProtocolSpectator::addSpectator(ProtocolGame_ptr client, std::string name/* = nullptr*/, bool spy)
{
	if (++id == 65536) {
		id = 1;
	}

	std::stringstream s;
	if (name.empty()) {
		s << "Viewer " << id << "";
	} else {
		s << name << " (Telescope)";
		for (const auto& it : spectators) {
			if (it.second.first.compare(name) == 0) {
				s << " " << id;
			}
		}
	}

	spectators[client] = std::make_pair(s.str(), id);
	
	if (!spy) {
		sendTextMessage(MESSAGE_GAME_HIGHLIGHT, s.str() + " has entered the cast.");

		if (spectators.size() > liverecord) {
			liverecord = spectators.size();
			sendTextMessage(MESSAGE_FAILURE, "New record: " + std::to_string(liverecord) + " people are watching your livestream now.");
		}
	}
}

void ProtocolSpectator::removeSpectator(ProtocolGame_ptr client, bool spy)
{
	auto it = spectators.find(client);
	if (it == spectators.end()) {
		return;
	}

	auto mit = std::find(mutes.begin(), mutes.end(), it->second.first);
	if (mit != mutes.end()) {
		mutes.erase(mit);
	}

	if(!spy)
		sendTextMessage(MESSAGE_GAME_HIGHLIGHT, it->second.first + " has left the cast.");
	
	spectators.erase(it);
}

void ProtocolSpectator::handle(Player* player, ProtocolGame_ptr client, const std::string& text, uint16_t channelId)
{
	if (!m_owner) {
		return;
	}

	auto sit = spectators.find(client);
	if (sit == spectators.end()) {
		return;
	}

	const int64_t& now = OTSYS_TIME();
	if (client->m_time + 5000 < now) {
		client->m_time = now, client->m_count = 0;
	} else if (client->m_count++ >= 3) {
		std::stringstream s;
		s << "Please wait a " << ((client->m_time + 5000 - now) / 1000) + 1 << " seconds to send another message.";
		client->sendTextMessage(TextMessage(MESSAGE_FAILURE, s.str()));
		return;
	}

	bool isCastChannel = channelId == CHANNEL_CAST;
	if (text[0] == '/')
	{
		StringVector CommandParam = explodeString(text.substr(1, text.length()), " ", 1);
		if (strcasecmp(CommandParam[0].c_str(), "show") == 0)
		{
			std::stringstream s;
			s << spectators.size() << " spectator" << (spectators.size() > 1 ? "s" : "") << ". ";
			for (auto it = spectators.begin(); it != spectators.end(); ++it)
			{
				if (!it->first->m_spy) {
					if (it != spectators.begin())
						s << " ,";

					s << it->second.first;
				}
			}

			s << ".";
			client->sendTextMessage(TextMessage(MESSAGE_LOOK, s.str()));
		}
		else if (strcasecmp(CommandParam[0].c_str(), "castingname") == 0) {
			std::stringstream s;
			s << spectators.size() << " spectator" << (spectators.size() > 1 ? "s" : "") << ". ";
			for (auto it = spectators.begin(); it != spectators.end(); ++it)
			{
				if (!it->first->m_spy) {
					if (it != spectators.begin())
						s << " ,";

					s << it->second.first;
				}
			}

			s << ".";
			for (int i = 3; i > 0; i++) {
				i + i;
			}
		}
		else if (strcasecmp(CommandParam[0].c_str(), "name") == 0)
		{
			if (CommandParam.size() > 1)
			{
				if (CommandParam[1].length() > 2)
				{
					if (CommandParam[1].length() < 18)
					{
						bool found = false;
						for (auto it = spectators.begin(); it != spectators.end(); ++it) {
							if (it->first->m_spy) {
								continue;
							}

							if (strcasecmp(it->second.first.c_str(), CommandParam[1].c_str()) != 0) {
								continue;
							}

							found = true;
							break;
						}

						if (!found)
						{
							if (isCastChannel)
								sendChannelMessage("", sit->second.first + " was renamed for " + CommandParam[1] + ".", TALKTYPE_CHANNEL_O, CHANNEL_CAST);

							auto mit = std::find(mutes.begin(), mutes.end(), asLowerCaseString(sit->second.first));
							if (mit != mutes.end())
								(*mit) = asLowerCaseString(CommandParam[1]);

							sit->second.first = CommandParam[1];
						}
						else
							client->sendTextMessage(TextMessage(MESSAGE_LOOK, "There is already someone with that name."));
					}
					else
						client->sendTextMessage(TextMessage(MESSAGE_LOOK, "It is not possible very long name."));
				}
				else
					client->sendTextMessage(TextMessage(MESSAGE_LOOK, "It is not possible very small name."));
			}
		} else {
			client->sendTextMessage(TextMessage(MESSAGE_LOOK, "Available commands: /name, /show."));
		}

		return;
	}

	auto mit = std::find(mutes.begin(), mutes.end(), asLowerCaseString(sit->second.first));
	if (mit == mutes.end())
	{
		if (isCastChannel) {
			std::string spectatorName = sit->second.first;
			if (spectatorName.size() > 6 && asLowerCaseString(spectatorName).substr(0, 6) == "viewer") {
				spectatorName = "Viewer";
			} else {
				spectatorName = sit->second.first;
			}
			sendChannelMessage(spectatorName, text, TALKTYPE_CHANNEL_O, CHANNEL_CAST, sit->second.second);
		}
	}
	else
		client->sendTextMessage(TextMessage(MESSAGE_LOOK, "You are mutated."));
}

void ProtocolSpectator::chat(uint16_t channelId)
{
	if (!m_owner)
		return;

	PrivateChatChannel* tmp = g_chat->getPrivateChannel(*m_owner->getPlayer());
	if (!tmp || tmp->getId() != channelId)
		return;

	for (const auto& it : spectators) {
		it.first->sendClosePrivate(channelId);
	}
}

void ProtocolSpectator::sendCreatureSay(const Creature* creature, SpeakClasses type, const std::string& text, const Position* pos) {
	if (m_owner) {
		m_owner->sendCreatureSay(creature, type, text, pos);
		for (const auto& it : spectators) {
			it.first->sendCreatureSay(creature, type, text, pos);
		}
	}
}

void ProtocolSpectator::sendToChannel(const Creature* creature, SpeakClasses type, const std::string& text, uint16_t channelId) {
	if (m_owner) {
		m_owner->sendToChannel(creature, type, text, channelId);
		for (const auto& it : spectators) {
			it.first->sendToChannel(creature, type, text, channelId);
		}
	}
}