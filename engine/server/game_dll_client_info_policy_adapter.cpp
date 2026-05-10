#include "game_dll_client_info_policy_adapter.h"

#include "engine/server/game_dll_client_info_policy.hpp"

namespace
{

int ToLegacyRoute(xash::engine::server::GameDllInfoBufferRoute route)
{
	using xash::engine::server::GameDllInfoBufferRoute;

	switch (route)
	{
	case GameDllInfoBufferRoute::LocalInfo:
		return SV_GAMEDLL_INFO_BUFFER_LOCALINFO;
	case GameDllInfoBufferRoute::ServerInfo:
		return SV_GAMEDLL_INFO_BUFFER_SERVERINFO;
	case GameDllInfoBufferRoute::ClientUserinfo:
		return SV_GAMEDLL_INFO_BUFFER_CLIENT_USERINFO;
	case GameDllInfoBufferRoute::EmptyString:
	default:
		return SV_GAMEDLL_INFO_BUFFER_EMPTY_STRING;
	}
}

int ToLegacySetValueAction(xash::engine::server::GameDllSetValueAction action)
{
	using xash::engine::server::GameDllSetValueAction;

	switch (action)
	{
	case GameDllSetValueAction::SetLocalInfo:
		return SV_GAMEDLL_SET_VALUE_LOCALINFO;
	case GameDllSetValueAction::SetServerInfo:
		return SV_GAMEDLL_SET_VALUE_SERVERINFO;
	case GameDllSetValueAction::PrintClientKeyError:
	default:
		return SV_GAMEDLL_SET_VALUE_PRINT_CLIENT_KEY_ERROR;
	}
}

int ToLegacyClientKeyAction(
	xash::engine::server::GameDllClientKeyValueAction action)
{
	using xash::engine::server::GameDllClientKeyValueAction;

	switch (action)
	{
	case GameDllClientKeyValueAction::SkipProtectedInfo:
		return SV_GAMEDLL_CLIENT_KEY_SKIP_PROTECTED_INFO;
	case GameDllClientKeyValueAction::SkipInvalidClient:
		return SV_GAMEDLL_CLIENT_KEY_SKIP_INVALID_CLIENT;
	case GameDllClientKeyValueAction::SkipUnchanged:
		return SV_GAMEDLL_CLIENT_KEY_SKIP_UNCHANGED;
	case GameDllClientKeyValueAction::UpdateAndResend:
	default:
		return SV_GAMEDLL_CLIENT_KEY_UPDATE_AND_RESEND;
	}
}

int ToLegacyClientStringAction(
	xash::engine::server::GameDllClientStringAction action)
{
	using xash::engine::server::GameDllClientStringAction;

	switch (action)
	{
	case GameDllClientStringAction::ReturnValue:
		return SV_GAMEDLL_CLIENT_STRING_RETURN_VALUE;
	case GameDllClientStringAction::PrintNonClientReturnEmpty:
		return SV_GAMEDLL_CLIENT_STRING_PRINT_NON_CLIENT_RETURN_EMPTY;
	case GameDllClientStringAction::PrintNonClientSkip:
	default:
		return SV_GAMEDLL_CLIENT_STRING_PRINT_NON_CLIENT_SKIP;
	}
}

int ToLegacyQueryCvarAction(
	xash::engine::server::GameDllQueryCvarAction action)
{
	using xash::engine::server::GameDllQueryCvarAction;

	switch (action)
	{
	case GameDllQueryCvarAction::SkipEmptyName:
		return SV_GAMEDLL_QUERY_CVAR_SKIP_EMPTY_NAME;
	case GameDllQueryCvarAction::SendQuery:
		return SV_GAMEDLL_QUERY_CVAR_SEND_QUERY;
	case GameDllQueryCvarAction::NotifyBadPlayer:
	default:
		return SV_GAMEDLL_QUERY_CVAR_NOTIFY_BAD_PLAYER;
	}
}

int ToLegacyGameDirAction(xash::engine::server::GameDllGameDirAction action)
{
	using xash::engine::server::GameDllGameDirAction;

	switch (action)
	{
	case GameDllGameDirAction::WriteFullPath:
		return SV_GAMEDLL_GAME_DIR_WRITE_FULL_PATH;
	case GameDllGameDirAction::WriteGameFolder:
	default:
		return SV_GAMEDLL_GAME_DIR_WRITE_GAME_FOLDER;
	}
}

}

extern "C" int SV_GameDllClientInfo_BuildInfoBufferRoute(
	int valid_edict,
	int world_edict,
	int has_client)
{
	return ToLegacyRoute(
		xash::engine::server::BuildGameDllInfoBufferRoute(
			valid_edict != 0,
			world_edict != 0,
			has_client != 0));
}

extern "C" sv_gamedll_set_value_plan_t
SV_GameDllClientInfo_BuildSetValuePlan(
	int local_info_buffer,
	int server_info_buffer,
	int max_local_info_length,
	int max_server_info_length)
{
	const xash::engine::server::GameDllSetValuePlan modern =
		xash::engine::server::BuildGameDllSetValuePlan(
			local_info_buffer != 0,
			server_info_buffer != 0,
			max_local_info_length,
			max_server_info_length);

	sv_gamedll_set_value_plan_t legacy = {};
	legacy.action = ToLegacySetValueAction(modern.action);
	legacy.max_length = modern.maxLength;
	return legacy;
}

extern "C" sv_gamedll_client_key_value_plan_t
SV_GameDllClientInfo_BuildClientKeyValuePlan(
	int protected_info_buffer,
	int clients_available,
	int client_index_one_based,
	int max_clients,
	int value_changed)
{
	const xash::engine::server::GameDllClientKeyValuePlan modern =
		xash::engine::server::BuildGameDllClientKeyValuePlan(
			protected_info_buffer != 0,
			clients_available != 0,
			client_index_one_based,
			max_clients,
			value_changed != 0);

	sv_gamedll_client_key_value_plan_t legacy = {};
	legacy.action = ToLegacyClientKeyAction(modern.action);
	legacy.client_index = modern.clientIndex;
	return legacy;
}

extern "C" int SV_GameDllClientInfo_BuildClientStringAction(int has_client)
{
	return ToLegacyClientStringAction(
		xash::engine::server::BuildGameDllClientStringAction(has_client != 0));
}

extern "C" int SV_GameDllClientInfo_BuildClientMutationAction(int has_client)
{
	return ToLegacyClientStringAction(
		xash::engine::server::BuildGameDllClientMutationAction(has_client != 0));
}

extern "C" int SV_GameDllClientInfo_BuildPlayerUserId(
	int has_client,
	int user_id)
{
	return xash::engine::server::BuildGameDllPlayerUserId(
		has_client != 0,
		user_id);
}

extern "C" sv_gamedll_player_stats_t SV_GameDllClientInfo_BuildPlayerStats(
	int has_client,
	float latency,
	int packet_loss)
{
	const xash::engine::server::GameDllPlayerStats modern =
		xash::engine::server::BuildGameDllPlayerStats(
			has_client != 0,
			latency,
			packet_loss);

	sv_gamedll_player_stats_t legacy = {};
	legacy.ping = modern.ping;
	legacy.packet_loss = modern.packetLoss;
	return legacy;
}

extern "C" int SV_GameDllClientInfo_BuildQueryCvarAction(
	const char *cvar_name,
	int has_client)
{
	return ToLegacyQueryCvarAction(
		xash::engine::server::BuildGameDllQueryCvarAction(
			cvar_name,
			has_client != 0));
}

extern "C" int SV_GameDllClientInfo_BuildGameDirAction(
	int full_path_compatibility,
	int root_available,
	int full_path_fits)
{
	return ToLegacyGameDirAction(
		xash::engine::server::BuildGameDllGameDirAction(
			full_path_compatibility != 0,
			root_available != 0,
			full_path_fits != 0));
}
