#ifndef XASH_ENGINE_SERVER_CLIENT_SESSION_SLOTS_ADAPTER_H
#define XASH_ENGINE_SERVER_CLIENT_SESSION_SLOTS_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#define SV_CLIENT_SESSION_SLOT_FREE 0
#define SV_CLIENT_SESSION_SLOT_ZOMBIE 1
#define SV_CLIENT_SESSION_SLOT_CONNECTED 2
#define SV_CLIENT_SESSION_SLOT_SPAWNING 3
#define SV_CLIENT_SESSION_SLOT_SPAWNED 4

#define SV_CLIENT_SESSION_MASTER_UPDATE_NONE 0
#define SV_CLIENT_SESSION_MASTER_UPDATE_FIRST_CONNECTED_CLIENT 1
#define SV_CLIENT_SESSION_MASTER_UPDATE_FULL_SERVER 2
#define SV_CLIENT_SESSION_MASTER_UPDATE_EMPTY_SERVER 3

typedef struct sv_client_session_slot_snapshot_s
{
	int state;
	unsigned int flags;
} sv_client_session_slot_snapshot_t;

typedef struct sv_client_session_population_s
{
	int players;
	int bots;
	int connected;
} sv_client_session_population_t;

int SV_ClientSession_SlotIsFree(int state);
int SV_ClientSession_SlotIsConnected(int state);
int SV_ClientSession_SlotIsFakeClient(unsigned int flags);

sv_client_session_population_t SV_ClientSession_CountPopulation(
	const sv_client_session_slot_snapshot_t *slots,
	int slot_count);

int SV_ClientSession_FindFirstFreeSlot(
	const sv_client_session_slot_snapshot_t *slots,
	int slot_count);

int SV_ClientSession_BuildConnectMasterUpdate(
	int connected_slots,
	int max_slots);

int SV_ClientSession_BuildDropMasterUpdate(int connected_slots);

#ifdef __cplusplus
}
#endif

#endif
