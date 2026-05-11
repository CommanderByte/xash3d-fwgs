#include "source_query_adapter.h"

#include "client_adapter_shared.hpp"
#include "common.h"
#include "engine/server/client/source_query.hpp"

#include <cstddef>

using xash::engine::server::adapter::client::FromLegacyBool;
using xash::engine::server::adapter::client::ToLegacyBool;
using xash::engine::server::adapter::client::ToLegacySize;

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
	return ToLegacyBool(xash::engine::server::SourceQueryAllowsPlayerList(
		FromLegacyBool(expose_player_list),
		FromLegacyBool(password_protected)));
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
	modern.passwordProtected = FromLegacyBool(details->password_protected);
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
	rule.isProtected = FromLegacyBool(is_protected);

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
