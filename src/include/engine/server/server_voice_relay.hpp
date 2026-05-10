#ifndef XASH_ENGINE_SERVER_SERVER_VOICE_RELAY_HPP
#define XASH_ENGINE_SERVER_SERVER_VOICE_RELAY_HPP

#include "engine/network/network_buffer.hpp"

#include <cstddef>
#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr std::size_t kVoiceRelayMaxIncomingPayloadBytes = 4096;
constexpr int kVoiceRelayDatagramCapacityOverheadBytes = 6;
constexpr std::uint8_t kVoiceRelayServiceCommand = 53;

enum class VoiceRelayStopReason
{
	None,
	PayloadTooLarge,
	VoiceDisabled,
	SenderNotSpawned,
	PhysicsHandled,
	SinglePlayerSuppressed,
};

struct VoiceInputGateRequest
{
	bool voiceEnabled;
	bool senderSpawned;
};

struct VoiceRelayDecision
{
	bool shouldRelay;
	VoiceRelayStopReason reason;
};

struct VoicePostPhysicsGateRequest
{
	bool physicsHandled;
	int maxClients;
	bool voiceSingleplayer;
};

enum class VoiceRecipientSkipReason
{
	None,
	RecipientNotConnected,
	ListenerDisabled,
	DatagramTooSmall,
};

struct VoiceRecipientRequest
{
	int senderIndex;
	int recipientIndex;
	bool recipientConnected;
	std::uint32_t listenerMask;
	bool loopbackRequested;
	unsigned int payloadSize;
	int datagramBytesLeft;
};

struct VoiceRecipientDecision
{
	bool shouldSend;
	unsigned int outgoingPayloadSize;
	VoiceRecipientSkipReason reason;
};

bool IsVoicePayloadTooLarge(unsigned int payloadSize);
VoiceRelayDecision BuildVoiceInputGateDecision(
	const VoiceInputGateRequest &request);
VoiceRelayDecision BuildVoicePostPhysicsGateDecision(
	const VoicePostPhysicsGateRequest &request);
VoiceRecipientDecision BuildVoiceRecipientDecision(
	const VoiceRecipientRequest &request);

void WriteVoiceDataPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	int senderIndex,
	unsigned int frames,
	const void *payload,
	unsigned int payloadSize);
void WriteVoiceDataMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	int senderIndex,
	unsigned int frames,
	const void *payload,
	unsigned int payloadSize);

}
}
}

#endif
