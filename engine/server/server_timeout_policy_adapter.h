#ifndef XASH_ENGINE_SERVER_TIMEOUT_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_TIMEOUT_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#define SV_TIMEOUT_CLIENT_ACTION_NONE 0
#define SV_TIMEOUT_CLIENT_ACTION_FREE_ZOMBIE 1
#define SV_TIMEOUT_CLIENT_ACTION_DROP_CONNECTED 2
#define SV_TIMEOUT_CLIENT_ACTION_DROP_SPAWNED 3

typedef struct sv_timeout_client_request_s
{
	int state;
	int fake_client;
	int has_entity;
	int entity_spectator;
	int entity_fake_client;
	int local_address;
	double connection_started;
	double last_received;
	double connected_drop_point;
	double spawned_drop_point;
	int ban_connected_timeout;
} sv_timeout_client_request_t;

typedef struct sv_timeout_client_plan_s
{
	int active_player;
	int action;
	int ban;
} sv_timeout_client_plan_t;

sv_timeout_client_plan_t SV_Timeout_BuildClientPlan(
	const sv_timeout_client_request_t *request);

int SV_Timeout_ShouldReleasePause(
	int max_clients,
	int paused,
	int active_players);

#ifdef __cplusplus
}
#endif

#endif
