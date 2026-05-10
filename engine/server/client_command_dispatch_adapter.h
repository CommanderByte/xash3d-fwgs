#ifndef XASH_ENGINE_SERVER_CLIENT_COMMAND_DISPATCH_ADAPTER_H
#define XASH_ENGINE_SERVER_CLIENT_COMMAND_DISPATCH_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sv_client_command_route_e
{
	SV_CLIENT_COMMAND_IGNORE = 0,
	SV_CLIENT_COMMAND_BUILTIN,
	SV_CLIENT_COMMAND_ENTTOOLS,
	SV_CLIENT_COMMAND_GAME_DLL,
	SV_CLIENT_COMMAND_FULLUPDATE,
} sv_client_command_route_t;

typedef struct sv_client_command_decision_s
{
	sv_client_command_route_t route;
	int command_index;
} sv_client_command_decision_t;

sv_client_command_decision_t SV_ClientCommandDispatch_Classify(
	const char *command_name,
	int server_active,
	int client_spawned,
	int enttools_enabled,
	int server_background,
	int fullupdate_throttled);

int SV_ClientCommandDispatch_BuiltinCount(void);
const char *SV_ClientCommandDispatch_BuiltinName(int index);

int SV_ClientCommandDispatch_EntToolsCount(void);
const char *SV_ClientCommandDispatch_EntToolsName(int index);

#ifdef __cplusplus
}
#endif

#endif
