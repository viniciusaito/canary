if not PlayersCasting then
	PlayersCasting = {}
end

local creatureEvent = CreatureEvent("CastLogout")
-- Cast Version = 0 for client 12 and 1 for client 10.
function creatureEvent.onLogout(player)
	db.query('UPDATE `players` set `cast_status` = 0, `cast_version` = 0, `cast_viewers` = 0, `cast_password` = "", `cast_description` = "" WHERE `id` = ' .. player:getGuid())
	db.query("DELETE FROM `live_casts` WHERE `player_id` = " .. player:getGuid())
	return true
end
creatureEvent:register()

local globalEvent = GlobalEvent("Cast")
function globalEvent.onThink(interval)
	for id in pairs(PlayersCasting) do
		local player = Player(id)
		if player then
			local data = player:getCast()
			if data.broadcast then
				local pdbid = db.storeQuery("SELECT `player_id` from `live_casts` where `player_id`=" .. player:getGuid())
				if not pdbid then
					db.query("INSERT IGNORE INTO `live_casts`(`player_id`, `cast_name`, `description`, `spectators`) SELECT `id`, `name`, `cast_description`, `cast_viewers` FROM `players` WHERE `id` = " .. player:getGuid())
				end
				db.query("UPDATE `players` set `cast_viewers` = " .. table.maxn(data.names) .. " WHERE `id` = " .. player:getGuid())
				db.query("UPDATE `live_casts` set `spectators` = " .. table.maxn(data.names) .. " WHERE `player_id` = " .. player:getGuid())
			end
		else
			PlayersCasting[id] = nil
		end
	end
	return true
end

globalEvent:interval(10000)
globalEvent:register()

local spy = TalkAction("/spy")
function spy.onSay(player, words, param)
	if not player:getGroup():getAccess() then
		return true
	end

	if param == "" then
		player:sendCancelMessage("Please type the player name.")
		return false
	end

	if param ~= player:getName():lower() then
		player:spySpectator(param)
	end
	return false
end

spy:separator(" ")
spy:groupType("normal")
spy:register()

local Help_Messages = {
	"Available commands:\n",
	"!cast on - enables the stream",
	"!cast off - disables the stream",
	"!cast desc, description - sets description about your cast",
	"!cast password, password - sets a password on the stream (use only numbers on cast password)",
	"!cast password off - disables the password protection",
	"!cast kick, name - kick a spectator from your stream",
	"!cast ban, name - locks spectator IP from joining your stream",
	"!cast unban, name - removes banishment lock",
	"!cast bans - shows banished spectators list",
	"!cast mute, name - mutes selected spectator from chat",
	"!cast unmute, name - removes mute",
	"!cast mutes - shows muted spectators list",
	"!cast show - displays the amount and nicknames of current spectators",
	"!cast status - displays stream status",
}

local MESSAGE_EVENT_DEFAULT = MESSAGE_FAILURE
local MESSAGE_INFO_DESCR = MESSAGE_LOOK
local MESSAGE_STATUS_CONSOLE_ORANGE = MESSAGE_GAME_HIGHLIGHT

local talk = TalkAction("!cast")
function talk.onSay(player, words, param)
	local CommandParam, data = param:splitTrimmed(","), player:getCast()
	local channelId = player:getPrivateChannelID()

	if isInArray({ "help" }, CommandParam[1]) then
		for _, message in pairs(Help_Messages) do
			player:sendChannelMessage("", message, TALKTYPE_CHANNEL_W, channelId)
		end
	elseif isInArray({ "off", "no", "disable" }, CommandParam[1]) then
		if data.broadcast == false then
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You already have the live stream closed.")
			return false
		end
		data.mutes = {}
		data.broadcast = false
		player:setCast(data)
		db.query("UPDATE `players` SET `cast_version` = 0, `cast_status` = 0, `cast_viewers` = 0 WHERE `id` = " .. player:getGuid())
		db.query("UPDATE `players` SET `cast_description` = '' WHERE `id` = " .. player:getGuid())
		db.query("DELETE FROM `live_casts` WHERE `player_id` = " .. player:getGuid())
		player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Your stream is currently disabled.")
		player:sendTextMessage(MESSAGE_INFO_DESCR, "Experience bonus deactivated: -10%")
		playersCasting[player:getId()] = nil
	elseif isInArray({ "on", "yes", "enable" }, CommandParam[1]) then
		if data.broadcast == true then
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You already have the live stream open.")
			return false
		end
		data.broadcast = true
		player:setCast(data)
		player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You have started live broadcast.")
		-- Checking if player is on client 11 or 10.
		if player:getClient().version <= 1200 then
			db.query("UPDATE `players` SET `cast_version` = 1100, `cast_status` = 1 WHERE `id` = " .. player:getGuid())
		else
			db.query("UPDATE `players` SET `cast_version` = 1220, `cast_status` = 1 WHERE `id` = " .. player:getGuid())
		end
		db.query("UPDATE `players` SET `cast_description` = '' WHERE `id` = " .. player:getGuid())
		if not pdbid then
			db.query("INSERT IGNORE INTO `live_casts`(`player_id`, `cast_name`, `description`, `spectators`, `version`) SELECT `id`, `name`, `cast_description`, `cast_viewers`, `cast_version` FROM `players` WHERE `id` = " .. player:getGuid())
		end
		player:sendTextMessage(MESSAGE_INFO_DESCR, "Experience bonus actived: +10%")
		playersCasting[player:getId()] = true
	elseif isInArray({ "show", "count", "see" }, CommandParam[1]) then
		if data.broadcast then
			local count = table.maxn(data.names)
			if count > 0 then
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You are currently watched by " .. count .. " people.")
				local str = ""
				for _, name in ipairs(data.names) do
					str = str .. (str:len() > 0 and ", " or "") .. name
				end

				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, str .. ".")
			else
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "None is watching your stream right now.")
			end
		else
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You are not streaming right now.")
		end
	elseif isInArray({ "kick", "remove" }, CommandParam[1]) then
		if data.broadcast then
			if CommandParam[2] then
				if CommandParam[2] ~= "all" then
					local found = false
					for _, name in ipairs(data.names) do
						if CommandParam[2]:lower() == name:lower() then
							found = true
							break
						end
					end

					if found then
						table.insert(data.kick, CommandParam[2])
						player:setCast(data)
						player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Spectator " .. CommandParam[2] .. " has been kicked.")
					else
						player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Spectator " .. CommandParam[2] .. " not found.")
					end
				else
					data.kick = data.names
					player:setCast(data)
					player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "All players has been kicked.")
				end
			else
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You need to type a name.")
			end
		else
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You are not streaming right now.")
		end
	elseif isInArray({ "ban", "block" }, CommandParam[1]) then
		if data.broadcast then
			if CommandParam[2] then
				local found = false
				for _, name in ipairs(data.names) do
					if CommandParam[2]:lower() == name:lower() then
						found = true
						break
					end
				end

				if found then
					table.insert(data.bans, CommandParam[2])
					player:setCast(data)
					player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Spectator " .. CommandParam[2] .. " has been banned.")
				else
					player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Spectator " .. CommandParam[2] .. " not found.")
				end
			else
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You need to type a name.")
			end
		else
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You are not streaming right now.")
		end
	elseif isInArray({ "unban", "unblock" }, CommandParam[1]) then
		if data.broadcast then
			if CommandParam[2] then
				local found, i = 0, 1
				for _, name in ipairs(data.bans) do
					if CommandParam[2]:lower() == name:lower() then
						found = i
						break
					end

					i = i + 1
				end

				if found > 0 then
					table.remove(data.bans, found)
					player:setCast(data)
					player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Spectator " .. CommandParam[2] .. " has been unbanned.")
				else
					player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Spectator " .. CommandParam[2] .. " not found.")
				end
			else
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You need to type a name.")
			end
		else
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You are not streaming right now.")
		end
	elseif isInArray({ "bans", "banlist" }, CommandParam[1]) then
		if table.maxn(data.bans) then
			local str = ""
			for _, name in ipairs(data.bans) do
				str = str .. (str:len() > 0 and ", " or "") .. name
			end

			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Currently banned spectators: " .. str .. ".")
		else
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Your ban list is empty.")
		end
	elseif isInArray({ "mute", "squelch" }, CommandParam[1]) then
		if data.broadcast then
			if CommandParam[2] then
				local found = false
				for _, name in ipairs(data.names) do
					if CommandParam[2]:lower() == name:lower() then
						found = true
						break
					end
				end

				if found then
					table.insert(data.mutes, CommandParam[2])
					player:setCast(data)
					player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Spectator " .. CommandParam[2] .. " has been muted.")
				else
					player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Spectator " .. CommandParam[2] .. " not found.")
				end
			else
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You need to type a name.")
			end
		else
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You are not streaming right now.")
		end
	elseif isInArray({ "unmute", "unsquelch" }, CommandParam[1]) then
		if data.broadcast then
			if CommandParam[2] then
				local found, i = 0, 1
				for _, name in ipairs(data.mutes) do
					if CommandParam[2]:lower() == name:lower() then
						found = i
						break
					end

					i = i + 1
				end

				if found > 0 then
					table.remove(data.mutes, found)
					player:setCast(data)
					player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Spectator " .. CommandParam[2] .. " has been unmuted.")
				else
					player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Spectator " .. CommandParam[2] .. " not found.")
				end
			else
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You need to type a name.")
			end
		else
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You are not streaming right now.")
		end
	elseif isInArray({ "mutes", "mutelist" }, CommandParam[1]) then
		if table.maxn(data.mutes) then
			local str = ""
			for _, name in ipairs(data.mutes) do
				str = str .. (str:len() > 0 and ", " or "") .. name
			end

			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Currently muted spectators: " .. str .. ".")
		else
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Your mute list is empty.")
		end
	elseif isInArray({ "password", "guard" }, CommandParam[1]) then
		if CommandParam[2] then
			if isInArray({ "off", "no", "disable" }, CommandParam[2]) then
				if data.password:len() ~= 0 then
					db.query("UPDATE `players` SET `cast_status` = 1 WHERE `id` = " .. player:getGuid())
					db.query("UPDATE `players` SET `cast_password` = '' WHERE `id` = " .. player:getGuid())
				end

				data.password = ""
				player:setCast(data)
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You have removed password for your stream.")
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Your experience bonus of 10% was reactivated.")
			else
				data.password = string.trim(CommandParam[2])
				if data.password:len() ~= 0 then
					db.query("UPDATE `players` SET `cast_status` = 3 WHERE `id` = " .. player:getGuid())
				end
				player:setCast(data)
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You have set new password for your stream (use only numbers in your cast password).")
				player:sendTextMessage(MESSAGE_STATUS_CONSOLE_ORANGE, "Attention, Your experience bonus of 10% was deactivated.")
				db.query("UPDATE `players` SET `cast_password` = " .. db.escapeString(data.password) .. " WHERE `id` = " .. player:getGuid())
				db.query("UPDATE `live_casts` SET `password` = " .. db.escapeString(data.password) .. " WHERE `player_id` = " .. player:getGuid())
			end
		elseif data.password ~= "" then
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Your stream is currently protected with password: " .. data.password .. ".")
		else
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Your stream is currently not protected.")
		end
	elseif isInArray({ "desc", "description" }, CommandParam[1]) then
		if CommandParam[2] then
			print(CommandParam[2])
			if isInArray({ "remove", "delete" }, CommandParam[2]) then
				db.query("UPDATE `players` SET `cast_description` = '' WHERE `id` = " .. player:getGuid())
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "You have removed description for your stream.")
				data.description = ""
				player:setCast(data)
			else
				if CommandParam[2]:match("[%a%d%s%u%l]+") ~= CommandParam[2] then
					player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Please only A-Z 0-9.")
					return false
				end
				if CommandParam[2]:len() > 0 and CommandParam[2]:len() <= 50 then
					db.query('UPDATE `players` SET `cast_description` = "' .. CommandParam[2] .. '" WHERE `id` = ' .. player:getGuid())
					db.query('UPDATE `live_casts` SET `description` = "' .. CommandParam[2] .. '" WHERE `player_id` = ' .. player:getGuid())
					data.description = CommandParam[2]
					player:setCast(data)
				else
					player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Your description max lenght 50 characters.")
					return false
				end
				player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Cast description was set to: " .. CommandParam[2] .. ".")
			end
		else
			player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Please enter your description or if you want to remove your description type remove.")
		end
	elseif isInArray({ "status", "info" }, CommandParam[1]) then
		player:sendTextMessage(MESSAGE_EVENT_DEFAULT, "Your stream is currently " .. (data.broadcast and "enabled" or "disabled") .. ".")
	else
		player:popupFYI(table.concat(Help_Messages, "\n"))
	end
	return false
end

talk:separator(" ")
spy:groupType("normal")
talk:register()
