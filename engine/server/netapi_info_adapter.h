#ifndef XASH_ENGINE_SERVER_NETAPI_INFO_ADAPTER_H
#define XASH_ENGINE_SERVER_NETAPI_INFO_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_legacy_server_info_s
{
	int request_protocol;
	int protocol_version;
	const char *hostname;
	const char *map_name;
	int deathmatch;
	int teamplay;
	int coop;
	int player_count;
	int max_players;
	const char *game_folder;
	int password_protected;
} sv_legacy_server_info_t;

typedef struct sv_netapi_details_s
{
	const char *hostname;
	const char *game_folder;
	int current_players;
	int max_players;
	const char *map_name;
} sv_netapi_details_t;

int SV_NetApiInfo_BuildLegacyServerInfo(
	char *out,
	unsigned int capacity,
	const sv_legacy_server_info_t *info);

int SV_NetApiInfo_BuildProtocolError(char *out, unsigned int capacity);
int SV_NetApiInfo_BuildUndefinedError(char *out, unsigned int capacity);
int SV_NetApiInfo_BuildForbiddenError(char *out, unsigned int capacity);
int SV_NetApiInfo_BuildPing(char *out, unsigned int capacity);

int SV_NetApiInfo_BeginRules(char *out, unsigned int capacity);
int SV_NetApiInfo_AppendRule(
	char *out,
	unsigned int capacity,
	const char *name,
	const char *value,
	int is_protected);
int SV_NetApiInfo_FinishRules(char *out, unsigned int capacity, int count);

int SV_NetApiInfo_BeginPlayers(char *out, unsigned int capacity);
int SV_NetApiInfo_AppendPlayer(
	char *out,
	unsigned int capacity,
	int index,
	const char *name,
	int frags,
	float time);
int SV_NetApiInfo_FinishPlayers(char *out, unsigned int capacity, int count);

int SV_NetApiInfo_BuildDetails(
	char *out,
	unsigned int capacity,
	const sv_netapi_details_t *details);

#ifdef __cplusplus
}
#endif

#endif
