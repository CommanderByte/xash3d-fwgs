#include "engine/server/server_sound_message.hpp"
#include "engine/server/server_message_envelope.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

int EncodeAttenuation(float attenuation)
{
	int value = static_cast<int>(attenuation * 64.0f);
	if (value > 255)
		value = 255;
	if (value < 0)
		value = 0;
	return value;
}

int RoundLikeLegacy(float value)
{
	return value < 0.0f ?
		static_cast<int>(value - 0.5f) :
		static_cast<int>(value + 0.5f);
}

int EncodeCoordinate(float value, bool largeCoordinates)
{
	if (largeCoordinates)
		return RoundLikeLegacy(value);

	return static_cast<int>(value * 8.0f);
}

void WriteCoordinate(
	xash::engine::network::NetworkBitBuffer &buffer,
	float value,
	bool largeCoordinates)
{
	buffer.writeSigned(EncodeCoordinate(value, largeCoordinates), 16);
}

}

SoundMessagePlan BuildSoundMessagePlan(
	int flags,
	int volume,
	float attenuation,
	int pitch)
{
	SoundMessagePlan plan = {};
	plan.command = (flags & kSoundMessageRestorePositionFlag) ?
		kSoundMessageRestoreCommand :
		kSoundMessageCommand;
	plan.networkFlags = flags;

	if (volume != kSoundMessageNormalVolume)
		plan.networkFlags |= kSoundMessageVolumeFlag;
	if (attenuation != kSoundMessageNoAttenuation)
		plan.networkFlags |= kSoundMessageAttenuationFlag;
	if (pitch != kSoundMessageNormalPitch)
		plan.networkFlags |= kSoundMessagePitchFlag;

	plan.networkFlags &= ~kSoundMessageRestorePositionFlag;
	plan.networkFlags &= ~kSoundMessageFilterClientFlag;
	plan.networkFlags &= ~kSoundMessageSpawningFlag;
	return plan;
}

int BuildSoundMessageChannel(
	int channel,
	const char *sample)
{
	if (sample && sample[0] == '*')
		return kSoundMessageStreamChannel;

	return channel;
}

void WriteSoundCommand(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint8_t command)
{
	WriteServerMessageCommand(buffer, command);
}

void WriteSoundPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const SoundMessagePayload &payload)
{
	buffer.writeUnsigned(static_cast<std::uint32_t>(payload.flags), kSoundMessageMaxFlagBits);
	buffer.writeUnsigned(static_cast<std::uint32_t>(payload.soundIndex), kSoundMessageMaxSoundBits);
	buffer.writeUnsigned(static_cast<std::uint32_t>(payload.channel), kSoundMessageMaxChannelBits);

	if (payload.flags & kSoundMessageVolumeFlag)
		WriteServerMessageByte(buffer, static_cast<unsigned int>(payload.volume));
	if (payload.flags & kSoundMessageAttenuationFlag)
		WriteServerMessageByte(
			buffer,
			static_cast<unsigned int>(EncodeAttenuation(payload.attenuation)));
	if (payload.flags & kSoundMessagePitchFlag)
		WriteServerMessageByte(buffer, static_cast<unsigned int>(payload.pitch));

	buffer.writeUnsigned(static_cast<std::uint32_t>(payload.entityIndex), kSoundMessageMaxEntityBits);
	WriteCoordinate(buffer, payload.origin[0], payload.largeCoordinates);
	WriteCoordinate(buffer, payload.origin[1], payload.largeCoordinates);
	WriteCoordinate(buffer, payload.origin[2], payload.largeCoordinates);
}

void WriteSoundMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const SoundMessagePayload &payload)
{
	const SoundMessagePlan plan = BuildSoundMessagePlan(
		payload.flags,
		payload.volume,
		payload.attenuation,
		payload.pitch);

	WriteSoundCommand(buffer, plan.command);

	SoundMessagePayload networkPayload = payload;
	networkPayload.flags = plan.networkFlags;
	WriteSoundPayload(buffer, networkPayload);
}


}
}
}
