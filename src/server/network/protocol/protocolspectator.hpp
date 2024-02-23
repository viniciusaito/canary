////////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
////////////////////////////////////////////////////////////////////////
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.	If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////

#pragma once

class Creature;
class Player;
class House;
class Container;
class Tile;
class Quest;

using ProtocolSpectatorList = std::map<ProtocolGame_ptr, std::pair<std::string, uint32_t>>;
using DataList = std::map<std::string, uint32_t>;

class ProtocolSpectator
{
public:
	ProtocolSpectator(ProtocolGame_ptr client) : m_owner(client)
	{
		id = 0;
		broadcast = false;
		broadcast_time = 0;
		liverecord = 0;
		description = "";
	}
	virtual ~ProtocolSpectator() {}

	void clear(bool full)
	{
		for (const auto& it : spectators) {
			if (!it.first->m_twatchername.empty()) {
				it.first->parseTelescopeBack(true);
				continue;
			}

			it.first->disconnect();
		}

		spectators.clear();
		mutes.clear();
		removeCaster();

		id = 0;
		if (!full)
			return;

		bans.clear();
		password = "";
		broadcast = false;
		broadcast_time = 0;
		liverecord = 0;
	}

	bool check(const std::string& _password);
	void handle(std::shared_ptr<Player> player, ProtocolGame_ptr client, const std::string& text, uint16_t channelId);
	void chat(uint16_t channelId);

	uint32_t getCastViewerCount() {
		return spectators.size();
	}

	StringVector list()
	{
		StringVector list_;
		for (const auto& it : spectators) {
			if (it.first && it.first->m_spy) {
				continue;
			}

			list_.push_back(it.second.first);
		}
		return list_;
	}

	void kick(StringVector list);
	StringVector muteList() { return mutes; }
	void mute(StringVector _mutes) { mutes = _mutes; }
	DataList banList() { return bans; }
	void ban(StringVector _bans);

	bool banned(uint32_t ip) const
	{
		for (const auto& it : bans) {
			if (it.second == ip) {
				return true;
			}
		}

		return false;
	}

	ProtocolGame_ptr getOwner() const { return m_owner; }
	void setOwner(ProtocolGame_ptr client) { m_owner = client; }
	void resetOwner() {
		m_owner.reset();
	}

	std::string getPassword() const { return password; }
	void setPassword(const std::string& value) { password = value; }

	bool isBroadcasting() const { return broadcast; }
	void setBroadcast(bool value) { broadcast = value; }

	std::string getBroadCastTimeString() const {
		std::stringstream broadcast;
		int64_t seconds = getBroadcastTime() / 1000;

		uint16_t hour = floor(seconds / 60 / 60 % 24);
		uint16_t minute = floor(seconds / 60 % 60);
		uint16_t second = floor(seconds % 60);

		if (hour > 0) { broadcast << hour << " hours, "; }
		if (minute > 0) { broadcast << minute << " minutes and "; }
		broadcast << second << " seconds.";
		return broadcast.str();
	}

	void addSpectator(ProtocolGame_ptr client, std::string name = "", bool m_spy = false);
	void removeSpectator(ProtocolGame_ptr client, bool m_spy = false);

	int64_t getBroadcastTime() const { return OTSYS_TIME() - broadcast_time; }
	void setBroadcastTime(int64_t time) { broadcast_time = time; }

	std::string getDescription() const { return description; }
	void setDescription(const std::string& desc) { description = desc; }

	uint32_t getSpectatorId(ProtocolGame_ptr client) const {
		auto it = spectators.find(client);
		if (it != spectators.end()) {
			return it->second.second;
		}
		return 0;
	}

	// inherited
	void insertCaster()
	{
		if (m_owner) {
			m_owner->insertCaster();
		}
	}

	void removeCaster();

	bool canSee(const Position& pos) const
	{
		if (m_owner) {
			return m_owner->canSee(pos);
		}

		return false;
	}

	uint32_t getIP() const
	{
		if (m_owner) {
			return m_owner->getIP();
		}

		return 0;
	}

	void sendStats()
	{
		if (m_owner) {
			m_owner->sendStats();

			for (const auto& it : spectators)
				it.first->sendStats();
		}
	}
	void sendPing()
	{
		if (m_owner) {
			m_owner->sendPing();

			for (const auto& it : spectators)
				it.first->sendPing();
		}
	}
	void logout(bool displayEffect, bool forceLogout)
	{
		if (m_owner) {
			m_owner->logout(displayEffect, forceLogout);
		}
	}
	void sendAddContainerItem(uint8_t cid, uint16_t slot, const std::shared_ptr<Item> item)
	{
		if (m_owner) {
			m_owner->sendAddContainerItem(cid, slot, item);

			for (const auto& it : spectators)
				it.first->sendAddContainerItem(cid, slot, item);
		}
	}
	void sendUpdateContainerItem(uint8_t cid, uint16_t slot, const std::shared_ptr<Item> item)
	{
		if (m_owner) {
			m_owner->sendUpdateContainerItem(cid, slot, item);

			for (const auto& it : spectators)
				it.first->sendUpdateContainerItem(cid, slot, item);
		}
	}
	void sendRemoveContainerItem(uint8_t cid, uint16_t slot, const std::shared_ptr<Item> lastItem)
	{
		if (m_owner) {
			m_owner->sendRemoveContainerItem(cid, slot, lastItem);

			for (const auto& it : spectators)
				it.first->sendRemoveContainerItem(cid, slot, lastItem);
		}
	}
	void sendUpdatedVIPStatus(uint32_t guid, VipStatus_t newStatus)
	{
		if (m_owner) {
			m_owner->sendUpdatedVIPStatus(guid, newStatus);

			for (const auto& it : spectators)
				it.first->sendUpdatedVIPStatus(guid, newStatus);
		}
	}
	void sendVIP(uint32_t guid, const std::string& name, const std::string& description, uint32_t icon, bool notify, VipStatus_t status)
	{
		if (m_owner) {
			m_owner->sendVIP(guid, name, description, icon, notify, status);

			for (const auto& it : spectators)
				it.first->sendVIP(guid, name, description, icon, notify, status);
		}
	}
	void sendClosePrivate(uint16_t channelId)
	{
		if (m_owner) {
			m_owner->sendClosePrivate(channelId);
		}
	}
	void sendFYIBox(const std::string& message) {
		if (m_owner) {
			m_owner->sendFYIBox(message);
		}
	}

	uint32_t getVersion() const {
		if (m_owner) {
			return m_owner->getVersion();
		}

		return 0;
	}

	void disconnect() {
		if (m_owner) {
			m_owner->disconnect();
		}
	}

	void sendPartyCreatureSkull(const std::shared_ptr<Creature> creature) const {
		if (m_owner) {
			m_owner->sendPartyCreatureSkull(creature);

			for (const auto& it : spectators)
				it.first->sendPartyCreatureSkull(creature);
		}
	}

	void sendAddTileItem(const Position& pos, uint32_t stackpos, const std::shared_ptr<Item> item) {
		if (m_owner) {
			m_owner->sendAddTileItem(pos, stackpos, item);

			for (const auto& it : spectators)
				it.first->sendAddTileItem(pos, stackpos, item);
		}
	}

	void sendUpdateTileItem(const Position& pos, uint32_t stackpos, const std::shared_ptr<Item> item) {
		if (m_owner) {
			m_owner->sendUpdateTileItem(pos, stackpos, item);

			for (const auto& it : spectators)
				it.first->sendUpdateTileItem(pos, stackpos, item);
		}
	}

	void sendRemoveTileThing(const Position& pos, int32_t stackpos) {
		if (m_owner) {
			m_owner->sendRemoveTileThing(pos, stackpos);

			for (const auto& it : spectators)
				it.first->sendRemoveTileThing(pos, stackpos);
		}
	}

	void sendUpdateTile(const Tile* tile, const Position& pos) {
		if (m_owner) {
			m_owner->sendUpdateTile(tile, pos);

			for (const auto& it : spectators)
				it.first->sendUpdateTile(tile, pos);
		}
	}

	void sendChannelMessage(const std::string& author, const std::string& text, SpeakClasses type, uint16_t channel, uint32_t spectatorLevel = 0) {
		if (m_owner) {
			m_owner->sendChannelMessage(author, text, type, channel, spectatorLevel);

			for (const auto& it : spectators)
				it.first->sendChannelMessage(author, text, type, channel, spectatorLevel);
		}
	}
	void sendMoveCreature(const std::shared_ptr<Creature> creature, const Position& newPos, int32_t newStackPos, const Position& oldPos, int32_t oldStackPos, bool teleport) {
		if (m_owner) {
			m_owner->sendMoveCreature(creature, newPos, newStackPos, oldPos, oldStackPos, teleport);

			for (const auto& it : spectators)
				it.first->sendMoveCreature(creature, newPos, newStackPos, oldPos, oldStackPos, teleport);
		}
	}
	void sendCreatureTurn(const std::shared_ptr<Creature> creature, int32_t stackpos) {
		if (m_owner) {
			m_owner->sendCreatureTurn(creature, stackpos);

			for (const auto& it : spectators)
				it.first->sendCreatureTurn(creature, stackpos);
		}
	}
	void sendCreatureSay(const std::shared_ptr<Creature> creature, SpeakClasses type, const std::string& text, const Position* pos = nullptr);
	void sendPrivateMessage(const std::shared_ptr<Player> speaker, SpeakClasses type, const std::string& text) {
		if (m_owner) {
			m_owner->sendPrivateMessage(speaker, type, text);
		}
	}
	void sendCreatureSquare(const std::shared_ptr<Creature> creature, SquareColor_t color) {
		if (m_owner) {
			m_owner->sendCreatureSquare(creature, color);

			for (const auto& it : spectators)
				it.first->sendCreatureSquare(creature, color);
		}
	}
	void sendCreatureOutfit(const std::shared_ptr<Creature> creature, const Outfit_t& outfit) {
		if (m_owner) {
			m_owner->sendCreatureOutfit(creature, outfit);

			for (const auto& it : spectators)
				it.first->sendCreatureOutfit(creature, outfit);
		}
	}
	void sendCreatureLight(const std::shared_ptr<Creature> creature) {
		if (m_owner) {
			m_owner->sendCreatureLight(creature);

			for (const auto& it : spectators)
				it.first->sendCreatureLight(creature);
		}
	}
	void sendCreatureWalkthrough(const std::shared_ptr<Creature> creature, bool walkthrough) {
		if (m_owner) {
			m_owner->sendCreatureWalkthrough(creature, walkthrough);

			for (const auto& it : spectators)
				it.first->sendCreatureWalkthrough(creature, walkthrough);
		}
	}
	void sendPartyCreatureShield(const std::shared_ptr<Creature> creature) {
		if (m_owner) {
			m_owner->sendPartyCreatureShield(creature);

			for (const auto& it : spectators)
				it.first->sendPartyCreatureShield(creature);
		}
	}
	void sendContainer(uint8_t cid, const Container* container, bool hasParent, uint16_t firstIndex) {
		if (m_owner) {
			m_owner->sendContainer(cid, container, hasParent, firstIndex);

			for (const auto& it : spectators)
				it.first->sendContainer(cid, container, hasParent, firstIndex);
		}
	}
	void sendInventoryItem(slots_t slot, const std::shared_ptr<Item> item) {
		if (m_owner) {
			m_owner->sendInventoryItem(slot, item);

			for (const auto& it : spectators)
				it.first->sendInventoryItem(slot, item);
		}
	}
	void sendCancelMessage(const std::string& msg) const {
		if (m_owner) {
			m_owner->sendTextMessage(TextMessage(MESSAGE_FAILURE, msg));

			for (const auto& it : spectators)
				it.first->sendTextMessage(TextMessage(MESSAGE_FAILURE, msg));
		}
	}
	void sendCancelTarget() const {
		if (m_owner) {
			m_owner->sendCancelTarget();

			for (const auto& it : spectators)
				it.first->sendCancelTarget();
		}
	}
	void sendCancelWalk() const {
		if (m_owner) {
			m_owner->sendCancelWalk();

			for (const auto& it : spectators)
				it.first->sendCancelWalk();
		}
	}
	void sendChangeSpeed(const std::shared_ptr<Creature> creature, uint32_t newSpeed) const {
		if (m_owner) {
			m_owner->sendChangeSpeed(creature, newSpeed);

			for (const auto& it : spectators)
				it.first->sendChangeSpeed(creature, newSpeed);
		}
	}
	void sendPartyPlayerVocation(const std::shared_ptr<Player> player) const {
		if (m_owner) {
			m_owner->sendPartyPlayerVocation(player);

			for (const auto& it : spectators)
				it.first->sendPartyPlayerVocation(player);		 
		}
	}
	void sendPlayerVocation(const std::shared_ptr<Player> player) const {
		if (m_owner) {
			m_owner->sendPlayerVocation(player);

			for (const auto& it : spectators)
				it.first->sendPlayerVocation(player);			
		}
	}		
	void sendCreatureHealth(const std::shared_ptr<Creature> creature) const {
		if (m_owner) {
			m_owner->sendCreatureHealth(creature);

			for (const auto& it : spectators)
				it.first->sendCreatureHealth(creature);
		}
	}
	void sendCreatureShield(const std::shared_ptr<Creature> creature) {
		if (m_owner) {
			m_owner->sendCreatureShield(creature);

			for (const auto& it : spectators)
				it.first->sendCreatureShield(creature);					 
		}
	}	
	void sendPartyCreatureUpdate(const std::shared_ptr<Creature> creature) const {
		if (m_owner) {
			m_owner->sendPartyCreatureUpdate(creature);

			for (const auto& it : spectators)
				it.first->sendPartyCreatureUpdate(creature);			
		}
	}
	void sendCreatureSkull(const std::shared_ptr<Creature> creature) const {
		if (m_owner) {
			m_owner->sendCreatureSkull(creature);

			for (const auto& it : spectators)
				it.first->sendCreatureSkull(creature);
		}	 
	}
	void sendPartyCreatureHealth(const std::shared_ptr<Creature> creature, uint8_t healthPercent) const {
		if (m_owner) {
			m_owner->sendPartyCreatureHealth(creature, healthPercent);

			for (const auto& it : spectators)
				it.first->sendPartyCreatureHealth(creature, healthPercent);		 
		}
	}
	void sendPartyPlayerMana(const std::shared_ptr<Player> player, uint8_t manaPercent) const {
		if (m_owner) {
			m_owner->sendPartyPlayerMana(player, manaPercent);

			for (const auto& it : spectators)
				it.first->sendPartyPlayerMana(player, manaPercent);			
		}
	}
	void sendPartyCreatureShowStatus(const std::shared_ptr<Creature> creature, bool showStatus) const {
		if (m_owner) {
			m_owner->sendPartyCreatureShowStatus(creature, showStatus);

			for (const auto& it : spectators)
				it.first->sendPartyCreatureShowStatus(creature, showStatus);				 
		}
	}			
	void sendDistanceShoot(const Position& from, const Position& to, unsigned char type) const {
		if (m_owner) {
			m_owner->sendDistanceShoot(from, to, type);

			for (const auto& it : spectators)
				it.first->sendDistanceShoot(from, to, type);
		}
	}
	void sendCreatePrivateChannel(uint16_t channelId, const std::string& channelName) {
		if (m_owner) {
			m_owner->sendCreatePrivateChannel(channelId, channelName);
		}
	}
	void sendIcons(uint16_t icons) const {
		if (m_owner) {
			m_owner->sendIcons(icons);

			for (const auto& it : spectators)
				it.first->sendIcons(icons);
		}
	}
	void sendMagicEffect(const Position& pos, uint8_t type) const {
		if (m_owner) {
			m_owner->sendMagicEffect(pos, type);

			for (const auto& it : spectators)
				it.first->sendMagicEffect(pos, type);
		}
	}
	void sendSkills() const {
		if (m_owner) {
			m_owner->sendSkills();

			for (const auto& it : spectators)
				it.first->sendSkills();
		}
	}
	void sendTextMessage(MessageClasses mclass, const std::string& message)
	{
		if (m_owner) {
			m_owner->sendTextMessage(TextMessage(mclass, message));

			for (const auto& it : spectators)
				it.first->sendTextMessage(TextMessage(mclass, message));
		}
	}
	void sendTextMessage(const TextMessage& message) const {
		if (m_owner) {
			m_owner->sendTextMessage(message);

			for (const auto& it : spectators)
				it.first->sendTextMessage(message);
		}
	}
	void sendReLoginWindow(uint8_t unfairFightReduction) {
		if (m_owner) {
			m_owner->sendReLoginWindow(unfairFightReduction);
			clear(true);
		}
	}
	void sendTextWindow(uint32_t windowTextId, std::shared_ptr<Item> item, uint16_t maxlen, bool canWrite) const {
		if (m_owner) {
			m_owner->sendTextWindow(windowTextId, item, maxlen, canWrite);
		}
	}
	void sendTextWindow(uint32_t windowTextId, uint32_t itemId, const std::string& text) const {
		if (m_owner) {
			m_owner->sendTextWindow(windowTextId, itemId, text);
		}
	}
	void sendToChannel(const std::shared_ptr<Creature> creature, SpeakClasses type, const std::string& text, uint16_t channelId);
	void sendShop(std::shared_ptr<Npc> npc) const {
		if (m_owner) {
			m_owner->sendShop(npc);
		}
	}
	void sendSaleItemList(const std::vector<ShopBlock> &shopVector, const std::map<uint16_t, uint16_t> &inventoryMap) const {
		if (m_owner) {
			m_owner->sendSaleItemList(shop, inventoryMap);
		}
	}
	void sendCloseShop() const {
		if (m_owner) {
			m_owner->sendCloseShop();
		}
	}
	void sendTradeItemRequest(const std::string& traderName, const std::shared_ptr<Item> item, bool ack) const {
		if (m_owner) {
			m_owner->sendTradeItemRequest(traderName, item, ack);
		}
	}
	void sendTradeClose() const {
		if (m_owner) {
			m_owner->sendCloseTrade();
		}
	}
	void sendWorldLight(const LightInfo& lightInfo) {
		if (m_owner) {
			m_owner->sendWorldLight(lightInfo);

			for (const auto& it : spectators)
				it.first->sendWorldLight(lightInfo);
		}
	}
	void sendChannelsDialog() {
		if (m_owner) {
			m_owner->sendChannelsDialog();
		}
	}
	void sendOpenPrivateChannel(const std::string& receiver) {
		if (m_owner) {
			m_owner->sendOpenPrivateChannel(receiver);
		}
	}
	void sendOutfitWindow() {
		if (m_owner) {
			m_owner->sendOutfitWindow();
		}
	}
	void sendCloseContainer(uint8_t cid) {
		if (m_owner) {
			m_owner->sendCloseContainer(cid);

			for (const auto& it : spectators)
				it.first->sendCloseContainer(cid);
			;
		}
	}
	void sendChannel(uint16_t channelId, const std::string& channelName, const UsersMap* channelUsers, const InvitedMap* invitedUsers) {
		if (m_owner) {
			m_owner->sendChannel(channelId, channelName, channelUsers, invitedUsers);
		}
	}
	void sendTutorial(uint8_t tutorialId) {
		if (m_owner) {
			m_owner->sendTutorial(tutorialId);
		}
	}
	void sendAddMarker(const Position& pos, uint8_t markType, const std::string& desc) {
		if (m_owner) {
			m_owner->sendAddMarker(pos, markType, desc);
		}
	}
	void sendFightModes() {
		if (m_owner) {
			m_owner->sendFightModes();
		}
	}
	void writeToOutputBuffer(const NetworkMessage& message) {
		if (m_owner) {
			m_owner->writeToOutputBuffer(message);
		}
	}
	void sendAddCreature(const std::shared_ptr<Creature> creature, const Position& pos, int32_t stackpos, bool isLogin)
	{
		if (m_owner) {
			m_owner->sendAddCreature(creature, pos, stackpos, isLogin);

			for (const auto& it : spectators)
				it.first->sendAddCreature(creature, pos, stackpos, isLogin);
		}
	}
	void sendHouseWindow(uint32_t windowTextId, const std::string& text)
	{
		if (m_owner) {
			m_owner->sendHouseWindow(windowTextId, text);
		}
	}

	void reloadCreature(const std::shared_ptr<Creature> creature) {
		if (m_owner) {
			m_owner->reloadCreature(creature);

			for (const auto& it : spectators)
				it.first->reloadCreature(creature);
		}
	}

	void telescopeGo(uint16_t guid, bool m_spy)
	{
		if (m_owner) {
			m_owner->telescopeGo(guid, m_spy);
		}
	}

	// MISSING
	void sendRestingStatus(uint8_t protection) {
		if (m_owner) {
			m_owner->sendRestingStatus(protection);
		}
	}
	void sendImbuementWindow(std::shared_ptr<Item> item) {
		if (m_owner) {
			m_owner->openImbuementWindow(item);
		}
	}
	void sendMarketEnter(uint32_t depotId) {
		if (m_owner) {
			m_owner->sendMarketEnter(depotId);
		}
	}
	void sendUnjustifiedPoints(const uint8_t& dayProgress, const uint8_t& dayLeft, const uint8_t& weekProgress, const uint8_t& weekLeft, const uint8_t& monthProgress, const uint8_t& monthLeft, const uint8_t& skullDuration) {
		if (m_owner) {
			m_owner->sendUnjustifiedPoints(dayProgress, dayLeft, weekProgress, weekLeft, monthProgress, monthLeft, skullDuration);
		}
	}
	void sendModalWindow(const ModalWindow& modalWindow) {
		if (m_owner) {
			m_owner->sendModalWindow(modalWindow);
		}
	}
	void AddItem(NetworkMessage& msg, const std::shared_ptr<Item> item) {
		if (m_owner) {
			m_owner->AddItem(msg, item);
		}
	}
	void BestiarysendCharms() {
		if (m_owner) {
			m_owner->BestiarysendCharms();
		}
	}
	void sendItemsPrice() {
		if (m_owner) {
			m_owner->sendItemsPrice();
		}
	}
	void sendBestiaryEntryChanged(uint16_t raceid) {
		if (m_owner) {
			m_owner->sendBestiaryEntryChanged(raceid);
		}
	}
	void refreshBestiaryTracker(std::list<MonsterType*> trackerList) {
		if (m_owner) {
			m_owner->refreshBestiaryTracker(trackerList);
		}
	}
	void sendChannelEvent(uint16_t channelId, const std::string& playerName, ChannelEvent_t channelEvent) {
		if (m_owner) {
			m_owner->sendChannelEvent(channelId, playerName, channelEvent);
		}
	}
	void sendCreatureIcons(const std::shared_ptr<Creature> creature) {
		if (m_owner) {
			m_owner->sendCreatureIcons(creature);
			for (const auto& it : spectators)
				it.first->sendCreatureIcons(creature);
		}
	}
	void sendCreatureType(const std::shared_ptr<Creature> creature, uint8_t creatureType) {
		if (m_owner) {
			m_owner->sendCreatureType(creature, creatureType);
			for (const auto& it : spectators)
				it.first->sendCreatureType(creature, creatureType);
		}
	}
	void sendCreatureHelpers(uint32_t creatureId, uint16_t helpers) {
		if (m_owner) {
			m_owner->sendCreatureHelpers(creatureId, helpers);
			for (const auto& it : spectators)
				it.first->sendCreatureHelpers(creatureId, helpers);
		}
	}	
	void sendSpellCooldown(uint8_t spellId, uint32_t time) {
		if (m_owner) {
			m_owner->sendSpellCooldown(spellId, time);
			for (const auto& it : spectators)
				it.first->sendSpellCooldown(spellId, time);
		}
	}
	void sendSpellGroupCooldown(SpellGroup_t groupId, uint32_t time) {
		if (m_owner) {
			m_owner->sendSpellGroupCooldown(groupId, time);
			for (const auto& it : spectators)
				it.first->sendSpellGroupCooldown(groupId, time);
		}
	}
	void sendLockerItems(std::map<uint16_t, uint16_t> itemMap, uint16_t count) {
		if (m_owner) {
			m_owner->sendLockerItems(itemMap, count);
		}
	}
	void sendCoinBalance() {
		if (m_owner) {
			m_owner->sendCoinBalance();
		}
	}
	void sendInventoryClientIds() {
		if (m_owner) {
			m_owner->sendInventoryClientIds();
		}
	}
	void sendOpenStore(uint8_t serviceType) {
		if (m_owner) {
			m_owner->sendOpenStore(serviceType);
		}
	}

	void sendStoreCategoryOffers(StoreCategory* category) {
		if (m_owner) {
			m_owner->sendStoreCategoryOffers(category);
		}
	}

	void sendStoreError(GameStoreError_t error, const std::string& errorMessage) {
		if (m_owner) {
			m_owner->sendStoreError(error, errorMessage);
		}
	}

	void sendStorePurchaseSuccessful(const std::string& message, const uint32_t newCoinBalance) {
		if (m_owner)
		{
			m_owner->sendStorePurchaseSuccessful(message, newCoinBalance);
		}
	}

	void sendStoreRequestAdditionalInfo(uint32_t offerId, ClientOffer_t clientOfferType) {
		if (m_owner) {
			m_owner->sendStoreRequestAdditionalInfo(offerId, clientOfferType);
		}
	}

	void sendStoreTrasactionHistory(HistoryStoreOfferList& list, uint32_t page, uint8_t entriesPerPage) {
		if (m_owner) {
			m_owner->sendStoreTrasactionHistory(list, page, entriesPerPage);
		}
	}
	void sendLootContainers() {
		if (m_owner) {
			m_owner->sendLootContainers();
		}
	}
	void sendLootStats(std::shared_ptr<Item> item, uint8_t count) {
		if (m_owner) {
			m_owner->sendLootStats(item, count);
		}
	}
	void sendClientCheck() const {
		if (m_owner) {
			m_owner->sendClientCheck();
		}
	}
	void sendGameNews() const {
		if (m_owner) {
			m_owner->sendGameNews();
		}
	}
	void sendPingBack() const {
		if (m_owner) {
			m_owner->sendPingBack();
			for (const auto& it : spectators)
				it.first->sendPingBack();
		}
	}
	void sendBasicData() const {
		if (m_owner) {
			m_owner->sendBasicData();
			for (const auto& it : spectators)
				it.first->sendBasicData();
		}
	}
	void sendBlessStatus() const {
		if (m_owner) {
			m_owner->sendBlessStatus();
			for (const auto& it : spectators)
				it.first->sendBlessStatus();
		}
	}
	void sendMarketLeave() {
		if (m_owner) {
			m_owner->sendMarketLeave();
		}
	}
	void sendMarketBrowseItem(uint16_t itemId, const MarketOfferList& buyOffers, const MarketOfferList& sellOffers, uint16_t tier) const {
		if (m_owner) {
			m_owner->sendMarketBrowseItem(itemId, buyOffers, sellOffers, tier);
		}
	}
	void sendMarketBrowseOwnOffers(const MarketOfferList& buyOffers, const MarketOfferList& sellOffers) const {
		if (m_owner) {
			m_owner->sendMarketBrowseOwnOffers(buyOffers, sellOffers);
		}
	}
	void sendMarketBrowseOwnHistory(const HistoryMarketOfferList& buyOffers, const HistoryMarketOfferList& sellOffers) const {
		if (m_owner) {
			m_owner->sendMarketBrowseOwnHistory(buyOffers, sellOffers);
		}
	}
	void sendMarketDetail(uint16_t itemId, uint16_t tier) const {
		if (m_owner) {
			m_owner->sendMarketDetail(itemId, tier);
		}
	}
	void sendMarketAcceptOffer(const MarketOfferEx& offer) const {
		if (m_owner) {
			m_owner->sendMarketAcceptOffer(offer);
		}
	}
	void sendMarketCancelOffer(const MarketOfferEx& offer) const {
		if (m_owner) {
			m_owner->sendMarketCancelOffer(offer);
		}
	}
	void sendCloseTrade() const {
		if (m_owner) {
			m_owner->sendCloseTrade();
		}
	}
	void sendTibiaTime(int32_t time) {
		if (m_owner) {
			m_owner->sendTibiaTime(time);
			for (const auto& it : spectators)
				it.first->sendTibiaTime(time);
		}
	}
	void sendSpecialContainersAvailable() {
		if (m_owner) {
			m_owner->sendSpecialContainersAvailable();
		}
	}
	void sendItemInspection(uint16_t itemId, uint8_t itemCount, const std::shared_ptr<Item> item, bool cyclopedia) {
		if (m_owner) {
			m_owner->sendItemInspection(itemId, itemCount, item, cyclopedia);
		}
	}
	void sendCyclopediaCharacterNoData(CyclopediaCharacterInfoType_t characterInfoType, uint8_t errorCode) {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterNoData(characterInfoType, errorCode);
		}
	}
	void sendCyclopediaCharacterBaseInformation() {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterBaseInformation();
		}
	}
	void sendCyclopediaCharacterGeneralStats() {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterGeneralStats();
		}
	}
	void sendCyclopediaCharacterCombatStats() {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterCombatStats();
		}
	}
	void sendCyclopediaCharacterRecentDeaths(uint16_t page, uint16_t pages, const std::vector<RecentDeathEntry>& entries) {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterRecentDeaths(page, pages, entries);
		}
	}
	void sendCyclopediaCharacterRecentPvPKills(uint16_t page, uint16_t pages, const std::vector<RecentPvPKillEntry>& entries) {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterRecentPvPKills(page, pages, entries);
		}
	}
	void sendCyclopediaCharacterAchievements() {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterAchievements();
		}
	}
	void sendCyclopediaCharacterItemSummary() {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterItemSummary();
		}
	}
	void sendCyclopediaCharacterOutfitsMounts() {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterOutfitsMounts();
		}
	}
	void sendCyclopediaCharacterStoreSummary() {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterStoreSummary();
		}
	}
	void sendCyclopediaCharacterInspection() {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterInspection();
		}
	}
	void sendCyclopediaCharacterBadges() {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterBadges();
		}
	}
	void sendCyclopediaCharacterTitles() {
		if (m_owner) {
			m_owner->sendCyclopediaCharacterTitles();
		}
	}
	void sendHighscoresNoData() {
		if (m_owner) {
			m_owner->sendHighscoresNoData();
		}
	}
	void sendHighscores(const std::vector<HighscoreCharacter>& characters, uint8_t categoryId, uint32_t vocationId, uint16_t page, uint16_t pages) {
		if (m_owner) {
			m_owner->sendHighscores(characters, categoryId, vocationId, page, pages);
		}
	}
	void sendTournamentLeaderboard() {
		if (m_owner) {
			m_owner->sendTournamentLeaderboard();
		}
	}
	void sendEnterWorld() {
		if (m_owner) {
			m_owner->sendEnterWorld();
			for (const auto& it : spectators)
				it.first->sendEnterWorld();
		}
	}
	void sendOpenStash() {
		if (m_owner) {
			m_owner->sendOpenStash();
		}
	}
	bool sendKillTrackerUpdate(Container* corpse, const std::string& playerName, const Outfit_t creatureOutfit) const
	{
		if (m_owner) {
			m_owner->sendKillTrackerUpdate(corpse, playerName, creatureOutfit);
			return true;
		}

		return false;
	}

	void sendUpdateSupplyTracker(const std::shared_ptr<Item> item) {
		if (m_owner) {
			m_owner->sendUpdateSupplyTracker(item);
		}
	}

	void sendUpdateImpactTracker(CombatType_t type, int32_t amount) {
		if (m_owner) {
			m_owner->sendUpdateImpactTracker(type, amount);
		}
	}
	void sendUpdateInputAnalyzer(CombatType_t type, int32_t amount, std::string target) {
		if (m_owner) {
			m_owner->sendUpdateInputAnalyzer(type, amount, target);
		}
	}
	void sendUpdateLootTracker(std::shared_ptr<Item> item)
	{
		if (m_owner) {
			m_owner->sendUpdateLootTracker(item);
		}
	}
	void createLeaderTeamFinder(NetworkMessage &msg)
	{
		if (m_owner) {
			m_owner->createLeaderTeamFinder(msg);
		}
	}
	void sendLeaderTeamFinder(bool reset)
	{
		if (m_owner) {
			m_owner->sendLeaderTeamFinder(reset);
		}
	}
	void sendTeamFinderList()
	{
		if (m_owner) {
			m_owner->sendTeamFinderList();
		}
	}

	void sendPodiumWindow(const std::shared_ptr<Item> podium, const Position& position, uint16_t spriteId, uint8_t stackpos) {
		if (m_owner) {
			m_owner->sendPodiumWindow(podium, position, spriteId, stackpos);
		}
	}

	using LiveCastsMap = std::unordered_map<std::shared_ptr<Player>, std::shared_ptr<ProtocolGame>>;
	const LiveCastsMap &getLiveCasts() const;

private:
	std::shared_ptr<Player> m_player;
	friend class Player;
	ProtocolSpectatorList spectators;
	StringVector mutes;
	DataList bans;
	Map map;

	ProtocolGame_ptr m_owner;
	uint32_t id;
	std::string password;
	std::string description;
	bool broadcast;
	int64_t broadcast_time;
	uint16_t liverecord;

	LiveCastsMap m_liveCasts;
};
