#ifndef XASH_ENGINE_SERVER_LIFECYCLE_LIMITS_ADAPTER_H
#define XASH_ENGINE_SERVER_LIFECYCLE_LIMITS_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

int SV_Lifecycle_ClampMaxClients(int requested_max_clients, int dedicated_server);
int SV_Lifecycle_UsesMultiplayerRules(int max_clients);
int SV_Lifecycle_SelectUpdateBackup(int max_clients);
int SV_Lifecycle_ClientEntityCount(
	int max_clients,
	int update_backup,
	int packet_entities_per_frame);
int SV_Lifecycle_DefaultClientEntityCount(
	int max_clients,
	int update_backup);
int SV_Lifecycle_GameEntityCount(int max_clients);
int SV_Lifecycle_SpawnSettlingFrameCount(int run_physics, int max_clients);
double SV_Lifecycle_SpawnSettlingFrameTime(int run_physics);

#ifdef __cplusplus
}
#endif

#endif
