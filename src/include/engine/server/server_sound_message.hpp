#ifndef XASH_ENGINE_SERVER_SERVER_SOUND_MESSAGE_HPP
#define XASH_ENGINE_SERVER_SERVER_SOUND_MESSAGE_HPP

#include "engine/network/network_buffer.hpp"

#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr std::uint8_t kSoundMessageCommand = 6;
constexpr std::uint8_t kSoundMessageRestoreCommand = 19;

constexpr int kSoundMessageMaxSoundBits = 11;
constexpr int kSoundMessageMaxEntityBits = 13;
constexpr int kSoundMessageMaxFlagBits = 14;
constexpr int kSoundMessageMaxChannelBits = 4;

constexpr int kSoundMessageVolumeFlag = 1 << 0;
constexpr int kSoundMessageAttenuationFlag = 1 << 1;
constexpr int kSoundMessageSequenceFlag = 1 << 2;
constexpr int kSoundMessagePitchFlag = 1 << 3;
constexpr int kSoundMessageSentenceFlag = 1 << 4;
constexpr int kSoundMessageStopFlag = 1 << 5;
constexpr int kSoundMessageSpawningFlag = 1 << 8;
constexpr int kSoundMessageFilterClientFlag = 1 << 11;
constexpr int kSoundMessageRestorePositionFlag = 1 << 12;

constexpr int kSoundMessageNormalVolume = 255;
constexpr int kSoundMessageNormalPitch = 100;
constexpr float kSoundMessageNoAttenuation = 0.0f;
constexpr int kSoundMessageStreamChannel = 5;

struct SoundMessagePlan
{
	std::uint8_t command;
	int networkFlags;
};

struct SoundMessagePayload
{
	int flags;
	int soundIndex;
	int channel;
	int volume;
	float attenuation;
	int pitch;
	int entityIndex;
	float origin[3];
	bool largeCoordinates;
};

SoundMessagePlan BuildSoundMessagePlan(
	int flags,
	int volume,
	float attenuation,
	int pitch);

int BuildSoundMessageChannel(
	int channel,
	const char *sample);

void WriteSoundCommand(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint8_t command);
void WriteSoundPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const SoundMessagePayload &payload);
void WriteSoundMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const SoundMessagePayload &payload);

}
}
}

#endif
