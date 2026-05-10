#ifndef XASH_ENGINE_SERVER_SERVER_SOUND_MESSAGE_ADAPTER_H
#define XASH_ENGINE_SERVER_SERVER_SOUND_MESSAGE_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_sound_message_plan_s
{
	int command;
	int flags;
} sv_sound_message_plan_t;

typedef struct sv_sound_message_write_result_s
{
	int current_bit;
	int overflow;
} sv_sound_message_write_result_t;

sv_sound_message_plan_t SV_SoundMessage_BuildPlan(
	int flags,
	int volume,
	float attenuation,
	int pitch);

int SV_SoundMessage_BuildChannel(
	int channel,
	const char *sample);

sv_sound_message_write_result_t SV_SoundMessage_WritePayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int flags,
	int sound_index,
	int channel,
	int volume,
	float attenuation,
	int pitch,
	int entity_index,
	float origin_x,
	float origin_y,
	float origin_z,
	int large_coordinates);

#ifdef __cplusplus
}
#endif

#endif
