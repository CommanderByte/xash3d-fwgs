#include "server_sound_message_adapter.h"

#include "engine/server/messaging/server_sound_message.hpp"
#include "server_message_adapter_shared.hpp"

extern "C" sv_sound_message_plan_t SV_SoundMessage_BuildPlan(
	int flags,
	int volume,
	float attenuation,
	int pitch)
{
	const xash::engine::server::SoundMessagePlan modern =
		xash::engine::server::BuildSoundMessagePlan(flags, volume, attenuation, pitch);

	sv_sound_message_plan_t legacy = {};
	legacy.command = modern.command;
	legacy.flags = modern.networkFlags;
	return legacy;
}

extern "C" int SV_SoundMessage_BuildChannel(
	int channel,
	const char *sample)
{
	return xash::engine::server::BuildSoundMessageChannel(channel, sample);
}

extern "C" sv_sound_message_write_result_t SV_SoundMessage_WritePayload(
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
	int large_coordinates)
{
	xash::engine::server::SoundMessagePayload payload = {};
	payload.flags = flags;
	payload.soundIndex = sound_index;
	payload.channel = channel;
	payload.volume = volume;
	payload.attenuation = attenuation;
	payload.pitch = pitch;
	payload.entityIndex = entity_index;
	payload.origin[0] = origin_x;
	payload.origin[1] = origin_y;
	payload.origin[2] = origin_z;
	payload.largeCoordinates = large_coordinates != 0;

	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);
	xash::engine::server::WriteSoundPayload(buffer, payload);
	return xash::engine::server::adapter::MakeWriteResult<
		sv_sound_message_write_result_t>(buffer);
}
