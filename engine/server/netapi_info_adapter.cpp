#include "netapi_info_adapter.h"

#include "engine/server/netapi_info.hpp"

namespace
{

int ToLegacyBool(bool value)
{
	return value ? 1 : 0;
}

}

extern "C" int SV_NetApiInfo_BuildLegacyServerInfo(
	char *out,
	unsigned int capacity,
	const sv_legacy_server_info_t *info)
{
	if (!info)
		return 0;

	xash::engine::server::LegacyServerInfo modern = {};
	modern.requestProtocol = info->request_protocol;
	modern.protocolVersion = info->protocol_version;
	modern.hostname = info->hostname;
	modern.mapName = info->map_name;
	modern.deathmatch = info->deathmatch != 0;
	modern.teamplay = info->teamplay != 0;
	modern.coop = info->coop != 0;
	modern.playerCount = info->player_count;
	modern.maxPlayers = info->max_players;
	modern.gameFolder = info->game_folder;
	modern.passwordProtected = info->password_protected != 0;

	return ToLegacyBool(xash::engine::server::BuildLegacyServerInfoString(
		out,
		capacity,
		modern));
}

extern "C" int SV_NetApiInfo_BuildProtocolError(char *out, unsigned int capacity)
{
	return ToLegacyBool(xash::engine::server::BuildNetApiProtocolError(out, capacity));
}

extern "C" int SV_NetApiInfo_BuildUndefinedError(char *out, unsigned int capacity)
{
	return ToLegacyBool(xash::engine::server::BuildNetApiUndefinedError(out, capacity));
}

extern "C" int SV_NetApiInfo_BuildForbiddenError(char *out, unsigned int capacity)
{
	return ToLegacyBool(xash::engine::server::BuildNetApiForbiddenError(out, capacity));
}

extern "C" int SV_NetApiInfo_BuildPing(char *out, unsigned int capacity)
{
	return ToLegacyBool(xash::engine::server::BuildNetApiPing(out, capacity));
}

extern "C" int SV_NetApiInfo_BeginRules(char *out, unsigned int capacity)
{
	return ToLegacyBool(xash::engine::server::BeginNetApiRules(out, capacity));
}

extern "C" int SV_NetApiInfo_AppendRule(
	char *out,
	unsigned int capacity,
	const char *name,
	const char *value,
	int is_protected)
{
	xash::engine::server::NetApiRuleInfo rule = {};
	rule.name = name;
	rule.value = value;
	rule.isProtected = is_protected != 0;

	return ToLegacyBool(xash::engine::server::AppendNetApiRule(out, capacity, rule));
}

extern "C" int SV_NetApiInfo_FinishRules(char *out, unsigned int capacity, int count)
{
	return ToLegacyBool(xash::engine::server::FinishNetApiRules(out, capacity, count));
}

extern "C" int SV_NetApiInfo_BeginPlayers(char *out, unsigned int capacity)
{
	return ToLegacyBool(xash::engine::server::BeginNetApiPlayers(out, capacity));
}

extern "C" int SV_NetApiInfo_AppendPlayer(
	char *out,
	unsigned int capacity,
	int index,
	const char *name,
	int frags,
	float time)
{
	xash::engine::server::NetApiPlayerInfo player = {};
	player.name = name;
	player.frags = frags;
	player.time = time;

	return ToLegacyBool(xash::engine::server::AppendNetApiPlayer(
		out,
		capacity,
		index,
		player));
}

extern "C" int SV_NetApiInfo_FinishPlayers(char *out, unsigned int capacity, int count)
{
	return ToLegacyBool(xash::engine::server::FinishNetApiPlayers(out, capacity, count));
}

extern "C" int SV_NetApiInfo_BuildDetails(
	char *out,
	unsigned int capacity,
	const sv_netapi_details_t *details)
{
	if (!details)
		return 0;

	xash::engine::server::NetApiDetailsInfo modern = {};
	modern.hostname = details->hostname;
	modern.gameFolder = details->game_folder;
	modern.currentPlayers = details->current_players;
	modern.maxPlayers = details->max_players;
	modern.mapName = details->map_name;

	return ToLegacyBool(xash::engine::server::BuildNetApiDetails(
		out,
		capacity,
		modern));
}
