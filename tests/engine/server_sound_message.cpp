#include <cstdlib>

#include "engine/network/network_buffer.hpp"
#include "engine/server/messaging/server_sound_message.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

static SoundMessagePayload BasicPayload()
{
	SoundMessagePayload payload = {};
	payload.flags = 0;
	payload.soundIndex = 42;
	payload.channel = 2;
	payload.volume = kSoundMessageNormalVolume;
	payload.attenuation = kSoundMessageNoAttenuation;
	payload.pitch = kSoundMessageNormalPitch;
	payload.entityIndex = 7;
	payload.origin[0] = 1.25f;
	payload.origin[1] = -2.5f;
	payload.origin[2] = 0.0f;
	payload.largeCoordinates = false;
	return payload;
}

static bool ReadSoundPayload(
	NetworkBitBuffer &reader,
	SoundMessagePayload *payload,
	int *encodedX,
	int *encodedY,
	int *encodedZ)
{
	payload->flags = static_cast<int>(reader.readUnsigned(kSoundMessageMaxFlagBits));
	payload->soundIndex = static_cast<int>(reader.readUnsigned(kSoundMessageMaxSoundBits));
	payload->channel = static_cast<int>(reader.readUnsigned(kSoundMessageMaxChannelBits));

	payload->volume = (payload->flags & kSoundMessageVolumeFlag) ?
		static_cast<int>(reader.readUnsigned(8)) :
		kSoundMessageNormalVolume;
	payload->attenuation = (payload->flags & kSoundMessageAttenuationFlag) ?
		static_cast<float>(reader.readUnsigned(8)) / 64.0f :
		kSoundMessageNoAttenuation;
	payload->pitch = (payload->flags & kSoundMessagePitchFlag) ?
		static_cast<int>(reader.readUnsigned(8)) :
		kSoundMessageNormalPitch;
	payload->entityIndex = static_cast<int>(reader.readUnsigned(kSoundMessageMaxEntityBits));
	*encodedX = reader.readSigned(16);
	*encodedY = reader.readSigned(16);
	*encodedZ = reader.readSigned(16);
	return !reader.overflow();
}

static bool TestPlanUsesSoundCommandAndNoOptionalFlagsForDefaults()
{
	const SoundMessagePlan plan = BuildSoundMessagePlan(
		0,
		kSoundMessageNormalVolume,
		kSoundMessageNoAttenuation,
		kSoundMessageNormalPitch);

	return plan.command == kSoundMessageCommand && plan.networkFlags == 0;
}

static bool TestPlanUsesRestoreCommandAndClearsLegacyOnlyFlags()
{
	const int inputFlags =
		kSoundMessageRestorePositionFlag |
		kSoundMessageFilterClientFlag |
		kSoundMessageSpawningFlag |
		kSoundMessageStopFlag;
	const SoundMessagePlan plan = BuildSoundMessagePlan(
		inputFlags,
		128,
		0.5f,
		90);

	return plan.command == kSoundMessageRestoreCommand &&
		(plan.networkFlags & kSoundMessageRestorePositionFlag) == 0 &&
		(plan.networkFlags & kSoundMessageFilterClientFlag) == 0 &&
		(plan.networkFlags & kSoundMessageSpawningFlag) == 0 &&
		(plan.networkFlags & kSoundMessageStopFlag) != 0 &&
		(plan.networkFlags & kSoundMessageVolumeFlag) != 0 &&
		(plan.networkFlags & kSoundMessageAttenuationFlag) != 0 &&
		(plan.networkFlags & kSoundMessagePitchFlag) != 0;
}

static bool TestStreamSampleForcesStreamChannel()
{
	return BuildSoundMessageChannel(2, "*stream.wav") == kSoundMessageStreamChannel &&
		BuildSoundMessageChannel(2, "weapons/test.wav") == 2 &&
		BuildSoundMessageChannel(2, nullptr) == 2;
}

static bool TestMinimalSoundMessageRoundTrip()
{
	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteSoundMessage(writer, BasicPayload());

	NetworkBitBuffer reader(data, writer.tellBit());
	SoundMessagePayload actual = {};
	int x = 0;
	int y = 0;
	int z = 0;

	return reader.readUnsigned(8) == kSoundMessageCommand &&
		ReadSoundPayload(reader, &actual, &x, &y, &z) &&
		actual.flags == 0 &&
		actual.soundIndex == 42 &&
		actual.channel == 2 &&
		actual.volume == kSoundMessageNormalVolume &&
		actual.attenuation == kSoundMessageNoAttenuation &&
		actual.pitch == kSoundMessageNormalPitch &&
		actual.entityIndex == 7 &&
		x == 10 &&
		y == -20 &&
		z == 0 &&
		reader.tellBit() == writer.tellBit() &&
		!writer.overflow();
}

static bool TestOptionalFieldsAndRestoreMessageRoundTrip()
{
	SoundMessagePayload payload = BasicPayload();
	payload.flags = kSoundMessageSentenceFlag | kSoundMessageSequenceFlag |
		kSoundMessageRestorePositionFlag | kSoundMessageFilterClientFlag |
		kSoundMessageSpawningFlag;
	payload.soundIndex = 12;
	payload.channel = 6;
	payload.volume = 128;
	payload.attenuation = 0.5f;
	payload.pitch = 88;
	payload.entityIndex = 8191;
	payload.origin[0] = 1.1f;
	payload.origin[1] = -1.1f;
	payload.origin[2] = 123.6f;
	payload.largeCoordinates = true;

	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteSoundMessage(writer, payload);

	NetworkBitBuffer reader(data, writer.tellBit());
	SoundMessagePayload actual = {};
	int x = 0;
	int y = 0;
	int z = 0;

	return reader.readUnsigned(8) == kSoundMessageRestoreCommand &&
		ReadSoundPayload(reader, &actual, &x, &y, &z) &&
		(actual.flags & kSoundMessageRestorePositionFlag) == 0 &&
		(actual.flags & kSoundMessageFilterClientFlag) == 0 &&
		(actual.flags & kSoundMessageSpawningFlag) == 0 &&
		(actual.flags & kSoundMessageSentenceFlag) != 0 &&
		(actual.flags & kSoundMessageSequenceFlag) != 0 &&
		(actual.flags & kSoundMessageVolumeFlag) != 0 &&
		(actual.flags & kSoundMessageAttenuationFlag) != 0 &&
		(actual.flags & kSoundMessagePitchFlag) != 0 &&
		actual.soundIndex == 12 &&
		actual.channel == 6 &&
		actual.volume == 128 &&
		actual.attenuation == 0.5f &&
		actual.pitch == 88 &&
		actual.entityIndex == 8191 &&
		x == 1 &&
		y == -1 &&
		z == 124 &&
		reader.tellBit() == writer.tellBit() &&
		!writer.overflow();
}

static bool TestAttenuationClampsToByte()
{
	SoundMessagePayload payload = BasicPayload();
	payload.flags = kSoundMessageAttenuationFlag;
	payload.attenuation = 4.0f;

	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteSoundPayload(writer, payload);

	NetworkBitBuffer reader(data, writer.tellBit());
	SoundMessagePayload actual = {};
	int x = 0;
	int y = 0;
	int z = 0;

	return ReadSoundPayload(reader, &actual, &x, &y, &z) &&
		actual.attenuation == (255.0f / 64.0f);
}

static bool TestPayloadCanAppendAfterLegacyCommand()
{
	unsigned char data[64] = {};
	data[0] = kSoundMessageCommand;

	NetworkBitBuffer writer(data, sizeof(data) << 3, 8);
	WriteSoundPayload(writer, BasicPayload());

	NetworkBitBuffer reader(data, writer.tellBit());
	SoundMessagePayload actual = {};
	int x = 0;
	int y = 0;
	int z = 0;

	return data[0] == kSoundMessageCommand &&
		reader.readUnsigned(8) == kSoundMessageCommand &&
		ReadSoundPayload(reader, &actual, &x, &y, &z) &&
		actual.soundIndex == 42 &&
		!writer.overflow();
}

static bool TestOverflow()
{
	unsigned char data[4] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteSoundMessage(writer, BasicPayload());
	return writer.overflow();
}

int main()
{
	if (!TestPlanUsesSoundCommandAndNoOptionalFlagsForDefaults() ||
		!TestPlanUsesRestoreCommandAndClearsLegacyOnlyFlags() ||
		!TestStreamSampleForcesStreamChannel() ||
		!TestMinimalSoundMessageRoundTrip() ||
		!TestOptionalFieldsAndRestoreMessageRoundTrip() ||
		!TestAttenuationClampsToByte() ||
		!TestPayloadCanAppendAfterLegacyCommand() ||
		!TestOverflow())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
