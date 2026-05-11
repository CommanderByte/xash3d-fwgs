#include "game_dll_client_info_policy_adapter.h"

#include "engine/server/game_dll/game_dll_client_info_policy.hpp"
#include "game_dll_adapter_shared.hpp"

using xash::engine::server::GameDllClientKeyValueAction;
using xash::engine::server::GameDllClientStringAction;
using xash::engine::server::GameDllGameDirAction;
using xash::engine::server::GameDllInfoBufferRoute;
using xash::engine::server::GameDllQueryCvarAction;
using xash::engine::server::GameDllSetValueAction;
using xash::engine::server::adapter::FromLegacyBool;
using xash::engine::server::adapter::ToLegacyEnum;

static_assert(SV_GAMEDLL_INFO_BUFFER_LOCALINFO ==
	static_cast<int>(GameDllInfoBufferRoute::LocalInfo),
	"GameDllInfoBufferRoute::LocalInfo value changed");
static_assert(SV_GAMEDLL_INFO_BUFFER_SERVERINFO ==
	static_cast<int>(GameDllInfoBufferRoute::ServerInfo),
	"GameDllInfoBufferRoute::ServerInfo value changed");
static_assert(SV_GAMEDLL_INFO_BUFFER_CLIENT_USERINFO ==
	static_cast<int>(GameDllInfoBufferRoute::ClientUserinfo),
	"GameDllInfoBufferRoute::ClientUserinfo value changed");
static_assert(SV_GAMEDLL_INFO_BUFFER_EMPTY_STRING ==
	static_cast<int>(GameDllInfoBufferRoute::EmptyString),
	"GameDllInfoBufferRoute::EmptyString value changed");
static_assert(SV_GAMEDLL_SET_VALUE_LOCALINFO ==
	static_cast<int>(GameDllSetValueAction::SetLocalInfo),
	"GameDllSetValueAction::SetLocalInfo value changed");
static_assert(SV_GAMEDLL_SET_VALUE_SERVERINFO ==
	static_cast<int>(GameDllSetValueAction::SetServerInfo),
	"GameDllSetValueAction::SetServerInfo value changed");
static_assert(SV_GAMEDLL_SET_VALUE_PRINT_CLIENT_KEY_ERROR ==
	static_cast<int>(GameDllSetValueAction::PrintClientKeyError),
	"GameDllSetValueAction::PrintClientKeyError value changed");
static_assert(SV_GAMEDLL_CLIENT_KEY_SKIP_PROTECTED_INFO ==
	static_cast<int>(GameDllClientKeyValueAction::SkipProtectedInfo),
	"GameDllClientKeyValueAction::SkipProtectedInfo value changed");
static_assert(SV_GAMEDLL_CLIENT_KEY_SKIP_INVALID_CLIENT ==
	static_cast<int>(GameDllClientKeyValueAction::SkipInvalidClient),
	"GameDllClientKeyValueAction::SkipInvalidClient value changed");
static_assert(SV_GAMEDLL_CLIENT_KEY_SKIP_UNCHANGED ==
	static_cast<int>(GameDllClientKeyValueAction::SkipUnchanged),
	"GameDllClientKeyValueAction::SkipUnchanged value changed");
static_assert(SV_GAMEDLL_CLIENT_KEY_UPDATE_AND_RESEND ==
	static_cast<int>(GameDllClientKeyValueAction::UpdateAndResend),
	"GameDllClientKeyValueAction::UpdateAndResend value changed");
static_assert(SV_GAMEDLL_CLIENT_STRING_RETURN_VALUE ==
	static_cast<int>(GameDllClientStringAction::ReturnValue),
	"GameDllClientStringAction::ReturnValue value changed");
static_assert(SV_GAMEDLL_CLIENT_STRING_PRINT_NON_CLIENT_RETURN_EMPTY ==
	static_cast<int>(GameDllClientStringAction::PrintNonClientReturnEmpty),
	"GameDllClientStringAction::PrintNonClientReturnEmpty value changed");
static_assert(SV_GAMEDLL_CLIENT_STRING_PRINT_NON_CLIENT_SKIP ==
	static_cast<int>(GameDllClientStringAction::PrintNonClientSkip),
	"GameDllClientStringAction::PrintNonClientSkip value changed");
static_assert(SV_GAMEDLL_QUERY_CVAR_SKIP_EMPTY_NAME ==
	static_cast<int>(GameDllQueryCvarAction::SkipEmptyName),
	"GameDllQueryCvarAction::SkipEmptyName value changed");
static_assert(SV_GAMEDLL_QUERY_CVAR_SEND_QUERY ==
	static_cast<int>(GameDllQueryCvarAction::SendQuery),
	"GameDllQueryCvarAction::SendQuery value changed");
static_assert(SV_GAMEDLL_QUERY_CVAR_NOTIFY_BAD_PLAYER ==
	static_cast<int>(GameDllQueryCvarAction::NotifyBadPlayer),
	"GameDllQueryCvarAction::NotifyBadPlayer value changed");
static_assert(SV_GAMEDLL_GAME_DIR_WRITE_GAME_FOLDER ==
	static_cast<int>(GameDllGameDirAction::WriteGameFolder),
	"GameDllGameDirAction::WriteGameFolder value changed");
static_assert(SV_GAMEDLL_GAME_DIR_WRITE_FULL_PATH ==
	static_cast<int>(GameDllGameDirAction::WriteFullPath),
	"GameDllGameDirAction::WriteFullPath value changed");

extern "C" int SV_GameDllClientInfo_BuildInfoBufferRoute(
	int valid_edict,
	int world_edict,
	int has_client)
{
	return ToLegacyEnum(
		xash::engine::server::BuildGameDllInfoBufferRoute(
			FromLegacyBool(valid_edict),
			FromLegacyBool(world_edict),
			FromLegacyBool(has_client)));
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
			FromLegacyBool(local_info_buffer),
			FromLegacyBool(server_info_buffer),
			max_local_info_length,
			max_server_info_length);

	sv_gamedll_set_value_plan_t legacy = {};
	legacy.action = ToLegacyEnum(modern.action);
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
			FromLegacyBool(protected_info_buffer),
			FromLegacyBool(clients_available),
			client_index_one_based,
			max_clients,
			FromLegacyBool(value_changed));

	sv_gamedll_client_key_value_plan_t legacy = {};
	legacy.action = ToLegacyEnum(modern.action);
	legacy.client_index = modern.clientIndex;
	return legacy;
}

extern "C" int SV_GameDllClientInfo_BuildClientStringAction(int has_client)
{
	return ToLegacyEnum(
		xash::engine::server::BuildGameDllClientStringAction(
			FromLegacyBool(has_client)));
}

extern "C" int SV_GameDllClientInfo_BuildClientMutationAction(int has_client)
{
	return ToLegacyEnum(
		xash::engine::server::BuildGameDllClientMutationAction(
			FromLegacyBool(has_client)));
}

extern "C" int SV_GameDllClientInfo_BuildPlayerUserId(
	int has_client,
	int user_id)
{
	return xash::engine::server::BuildGameDllPlayerUserId(
		FromLegacyBool(has_client),
		user_id);
}

extern "C" sv_gamedll_player_stats_t SV_GameDllClientInfo_BuildPlayerStats(
	int has_client,
	float latency,
	int packet_loss)
{
	const xash::engine::server::GameDllPlayerStats modern =
		xash::engine::server::BuildGameDllPlayerStats(
			FromLegacyBool(has_client),
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
	return ToLegacyEnum(
		xash::engine::server::BuildGameDllQueryCvarAction(
			cvar_name,
			FromLegacyBool(has_client)));
}

extern "C" int SV_GameDllClientInfo_BuildGameDirAction(
	int full_path_compatibility,
	int root_available,
	int full_path_fits)
{
	return ToLegacyEnum(
		xash::engine::server::BuildGameDllGameDirAction(
			FromLegacyBool(full_path_compatibility),
			FromLegacyBool(root_available),
			FromLegacyBool(full_path_fits)));
}
