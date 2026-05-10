#ifndef XASH_ENGINE_SERVER_SOURCE_QUERY_ADAPTER_H
#define XASH_ENGINE_SERVER_SOURCE_QUERY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_source_query_details_s
{
	int protocol_version;
	const char *hostname;
	const char *map_name;
	const char *game_folder;
	const char *game_description;
	int app_id;
	int player_count;
	int max_players;
	int bot_count;
	char server_type;
	char platform;
	int password_protected;
	int secure;
	const char *version;
} sv_source_query_details_t;

char SV_SourceQueryAdapter_PlatformCode(void);
int SV_SourceQueryAdapter_AllowsPlayerList(int expose_player_list, int password_protected);
int SV_SourceQueryAdapter_BuildDetails(
	void *buffer,
	unsigned int capacity,
	const sv_source_query_details_t *details);

int SV_SourceQueryAdapter_BeginRules(void *buffer, unsigned int capacity);
int SV_SourceQueryAdapter_AppendRule(
	void *buffer,
	unsigned int capacity,
	int offset,
	const char *name,
	const char *value,
	int is_protected);
int SV_SourceQueryAdapter_FinishRules(
	void *buffer,
	unsigned int capacity,
	int offset,
	unsigned int count);

int SV_SourceQueryAdapter_BeginPlayers(void *buffer, unsigned int capacity);
int SV_SourceQueryAdapter_AppendPlayer(
	void *buffer,
	unsigned int capacity,
	int offset,
	unsigned int index,
	const char *name,
	int frags,
	float duration);
int SV_SourceQueryAdapter_FinishPlayers(
	void *buffer,
	unsigned int capacity,
	int offset,
	unsigned int count);

#ifdef __cplusplus
}
#endif

#endif
