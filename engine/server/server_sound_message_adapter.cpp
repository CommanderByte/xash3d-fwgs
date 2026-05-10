#include "server_sound_message_adapter.h"

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_sound_message.hpp"

#include <cstddef>

namespace
{

xash::engine::network::NetworkBitBuffer MakeBuffer(
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	return xash::engine::network::NetworkBitBuffer(
		data,
		data_bits < 0 ? 0U : static_cast<std::size_t>(data_bits),
		current_bit < 0 ? 0U : static_cast<std::size_t>(current_bit));
}

sv_sound_message_write_result_t MakeResult(
	const xash::engine::network::NetworkBitBuffer &buffer)
{
	sv_sound_message_write_result_t result = {};
	result.current_bit = static_cast<int>(buffer.tellBit());
	result.overflow = buffer.overflow() ? 1 : 0;
	return result;
}

}

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
		MakeBuffer(data, data_bits, current_bit);
	xash::engine::server::WriteSoundPayload(buffer, payload);
	return MakeResult(buffer);
}
