#ifndef XASH_ENGINE_SERVER_GAME_DLL_CLIENT_INFO_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_GAME_DLL_CLIENT_INFO_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

enum sv_gamedll_info_buffer_route_e
{
	SV_GAMEDLL_INFO_BUFFER_LOCALINFO = 0,
	SV_GAMEDLL_INFO_BUFFER_SERVERINFO = 1,
	SV_GAMEDLL_INFO_BUFFER_CLIENT_USERINFO = 2,
	SV_GAMEDLL_INFO_BUFFER_EMPTY_STRING = 3
};

enum sv_gamedll_set_value_action_e
{
	SV_GAMEDLL_SET_VALUE_LOCALINFO = 0,
	SV_GAMEDLL_SET_VALUE_SERVERINFO = 1,
	SV_GAMEDLL_SET_VALUE_PRINT_CLIENT_KEY_ERROR = 2
};

enum sv_gamedll_client_key_value_action_e
{
	SV_GAMEDLL_CLIENT_KEY_SKIP_PROTECTED_INFO = 0,
	SV_GAMEDLL_CLIENT_KEY_SKIP_INVALID_CLIENT = 1,
	SV_GAMEDLL_CLIENT_KEY_SKIP_UNCHANGED = 2,
	SV_GAMEDLL_CLIENT_KEY_UPDATE_AND_RESEND = 3
};

enum sv_gamedll_client_string_action_e
{
	SV_GAMEDLL_CLIENT_STRING_RETURN_VALUE = 0,
	SV_GAMEDLL_CLIENT_STRING_PRINT_NON_CLIENT_RETURN_EMPTY = 1,
	SV_GAMEDLL_CLIENT_STRING_PRINT_NON_CLIENT_SKIP = 2
};

enum sv_gamedll_query_cvar_action_e
{
	SV_GAMEDLL_QUERY_CVAR_SKIP_EMPTY_NAME = 0,
	SV_GAMEDLL_QUERY_CVAR_SEND_QUERY = 1,
	SV_GAMEDLL_QUERY_CVAR_NOTIFY_BAD_PLAYER = 2
};

enum sv_gamedll_game_dir_action_e
{
	SV_GAMEDLL_GAME_DIR_WRITE_GAME_FOLDER = 0,
	SV_GAMEDLL_GAME_DIR_WRITE_FULL_PATH = 1
};

typedef struct sv_gamedll_set_value_plan_s
{
	int action;
	int max_length;
} sv_gamedll_set_value_plan_t;

typedef struct sv_gamedll_client_key_value_plan_s
{
	int action;
	int client_index;
} sv_gamedll_client_key_value_plan_t;

typedef struct sv_gamedll_player_stats_s
{
	int ping;
	int packet_loss;
} sv_gamedll_player_stats_t;

int SV_GameDllClientInfo_BuildInfoBufferRoute(
	int valid_edict,
	int world_edict,
	int has_client);
sv_gamedll_set_value_plan_t SV_GameDllClientInfo_BuildSetValuePlan(
	int local_info_buffer,
	int server_info_buffer,
	int max_local_info_length,
	int max_server_info_length);
sv_gamedll_client_key_value_plan_t
SV_GameDllClientInfo_BuildClientKeyValuePlan(
	int protected_info_buffer,
	int clients_available,
	int client_index_one_based,
	int max_clients,
	int value_changed);
int SV_GameDllClientInfo_BuildClientStringAction(int has_client);
int SV_GameDllClientInfo_BuildClientMutationAction(int has_client);
int SV_GameDllClientInfo_BuildPlayerUserId(int has_client, int user_id);
sv_gamedll_player_stats_t SV_GameDllClientInfo_BuildPlayerStats(
	int has_client,
	float latency,
	int packet_loss);
int SV_GameDllClientInfo_BuildQueryCvarAction(
	const char *cvar_name,
	int has_client);
int SV_GameDllClientInfo_BuildGameDirAction(
	int full_path_compatibility,
	int root_available,
	int full_path_fits);

#ifdef __cplusplus
}
#endif

#endif
