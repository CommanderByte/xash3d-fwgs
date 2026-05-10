#ifndef XASH_ENGINE_SERVER_SPAWN_HANDSHAKE_ADAPTER_H
#define XASH_ENGINE_SERVER_SPAWN_HANDSHAKE_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

enum sv_spawn_command_action_e
{
	SV_SPAWN_COMMAND_REJECT = 0,
	SV_SPAWN_COMMAND_SEND_SERVERDATA = 1,
	SV_SPAWN_COMMAND_RESEND_NEW = 2,
	SV_SPAWN_COMMAND_PUT_CLIENT_IN_SERVER = 3,
	SV_SPAWN_COMMAND_MARK_SPAWNED = 4
};

enum sv_spawn_overflow_action_e
{
	SV_SPAWN_OVERFLOW_NONE = 0,
	SV_SPAWN_OVERFLOW_HOST_ERROR = 1,
	SV_SPAWN_OVERFLOW_DROP_CLIENT = 2
};

typedef struct sv_spawn_handshake_write_result_s
{
	int current_bit;
	int overflow;
} sv_spawn_handshake_write_result_t;

int SV_Serverdata_ShouldEmitPrint(int developer_mode, int max_clients);
void SV_Serverdata_FormatPrintText(
	char *out,
	unsigned int out_size,
	int build_number,
	int progs_crc,
	int spawn_count);

sv_spawn_handshake_write_result_t SV_Serverdata_WritePayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int protocol_version,
	int spawn_count,
	unsigned int world_map_crc,
	int client_index,
	int max_clients,
	int max_edicts,
	int max_models,
	const char *map_name,
	const char *map_message,
	int background,
	const char *game_folder,
	unsigned int host_features,
	const float *player_mins,
	const float *player_maxs,
	int hull_count);

sv_spawn_handshake_write_result_t SV_SpawnHandshake_WriteSignonNumberMessage(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int signon_number);

enum sv_spawn_command_action_e SV_SpawnHandshake_BuildNewCommandAction(
	int client_connected);
enum sv_spawn_command_action_e SV_SpawnHandshake_BuildSpawnCommandAction(
	int client_connected,
	int requested_spawn_count,
	int current_spawn_count);
enum sv_spawn_command_action_e SV_SpawnHandshake_BuildBeginCommandAction(
	int client_spawning);
int SV_SpawnHandshake_BuildResendFlags(void);
int SV_SpawnHandshake_ShouldSendSignon(int fake_client);
enum sv_spawn_overflow_action_e SV_SpawnHandshake_BuildSignonOverflowAction(
	int overflowed,
	int max_clients);

#ifdef __cplusplus
}
#endif

#endif
