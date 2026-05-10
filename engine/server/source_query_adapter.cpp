#include "source_query_adapter.h"

#include "common.h"
#include "engine/server/source_query.hpp"

#include <cstddef>

namespace
{

int ToLegacySize(std::size_t value)
{
	if (value == 0 || value > static_cast<std::size_t>(0x7fffffff))
		return 0;

	return static_cast<int>(value);
}

}

extern "C" char SV_SourceQueryAdapter_PlatformCode(void)
{
#if XASH_WIN32
	return 'w';
#elif XASH_APPLE
	return 'm';
#else
	return 'l';
#endif
}

extern "C" int SV_SourceQueryAdapter_AllowsPlayerList(
	int expose_player_list,
	int password_protected)
{
	return xash::engine::server::SourceQueryAllowsPlayerList(
		expose_player_list != 0,
		password_protected != 0);
}

extern "C" int SV_SourceQueryAdapter_BuildDetails(
	void *buffer,
	unsigned int capacity,
	const sv_source_query_details_t *details)
{
	if (!details)
		return 0;

	xash::engine::server::SourceQueryDetails modern = {};
	modern.protocolVersion = details->protocol_version;
	modern.hostname = details->hostname;
	modern.mapName = details->map_name;
	modern.gameFolder = details->game_folder;
	modern.gameDescription = details->game_description;
	modern.appId = details->app_id;
	modern.playerCount = details->player_count;
	modern.maxPlayers = details->max_players;
	modern.botCount = details->bot_count;
	modern.serverType = details->server_type;
	modern.platform = details->platform;
	modern.passwordProtected = details->password_protected != 0;
	modern.secure = details->secure;
	modern.version = details->version;

	return ToLegacySize(xash::engine::server::BuildSourceQueryDetails(
		modern,
		buffer,
		capacity));
}

extern "C" int SV_SourceQueryAdapter_BeginRules(void *buffer, unsigned int capacity)
{
	return ToLegacySize(xash::engine::server::BeginSourceQueryRules(buffer, capacity));
}

extern "C" int SV_SourceQueryAdapter_AppendRule(
	void *buffer,
	unsigned int capacity,
	int offset,
	const char *name,
	const char *value,
	int is_protected)
{
	xash::engine::server::SourceQueryRule rule = {};
	rule.name = name;
	rule.value = value;
	rule.isProtected = is_protected != 0;

	return ToLegacySize(xash::engine::server::AppendSourceQueryRule(
		buffer,
		capacity,
		static_cast<std::size_t>(offset),
		rule));
}

extern "C" int SV_SourceQueryAdapter_FinishRules(
	void *buffer,
	unsigned int capacity,
	int offset,
	unsigned int count)
{
	return ToLegacySize(xash::engine::server::FinishSourceQueryRules(
		buffer,
		capacity,
		static_cast<std::size_t>(offset),
		count));
}

extern "C" int SV_SourceQueryAdapter_BeginPlayers(void *buffer, unsigned int capacity)
{
	return ToLegacySize(xash::engine::server::BeginSourceQueryPlayers(buffer, capacity));
}

extern "C" int SV_SourceQueryAdapter_AppendPlayer(
	void *buffer,
	unsigned int capacity,
	int offset,
	unsigned int index,
	const char *name,
	int frags,
	float duration)
{
	xash::engine::server::SourceQueryPlayer player = {};
	player.index = index;
	player.name = name;
	player.frags = frags;
	player.duration = duration;

	return ToLegacySize(xash::engine::server::AppendSourceQueryPlayer(
		buffer,
		capacity,
		static_cast<std::size_t>(offset),
		player));
}

extern "C" int SV_SourceQueryAdapter_FinishPlayers(
	void *buffer,
	unsigned int capacity,
	int offset,
	unsigned int count)
{
	return ToLegacySize(xash::engine::server::FinishSourceQueryPlayers(
		buffer,
		capacity,
		static_cast<std::size_t>(offset),
		count));
}
