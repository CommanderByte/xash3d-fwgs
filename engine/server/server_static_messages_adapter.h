#ifndef XASH_ENGINE_SERVER_SERVER_STATIC_MESSAGES_ADAPTER_H
#define XASH_ENGINE_SERVER_SERVER_STATIC_MESSAGES_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_static_message_write_result_s
{
	int current_bit;
	int overflow;
} sv_static_message_write_result_t;

typedef struct sv_spawn_static_decision_s
{
	int should_write;
	int reason;
} sv_spawn_static_decision_t;

enum
{
	SV_SPAWN_STATIC_REASON_NONE = 0,
	SV_SPAWN_STATIC_REASON_TOO_MANY_STATIC_ENTITIES = 1,
	SV_SPAWN_STATIC_REASON_BUFFER_TOO_SMALL = 2
};

sv_spawn_static_decision_t SV_StaticMessage_BuildSpawnStaticDecision(
	int index,
	int max_static_entities,
	int bytes_left);

sv_static_message_write_result_t SV_StaticMessage_WriteBspDecalPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	float origin_x,
	float origin_y,
	float origin_z,
	int decal_index,
	int entity_index,
	int model_index,
	int flags,
	float scale,
	int large_coordinates);

#ifdef __cplusplus
}
#endif

#endif
