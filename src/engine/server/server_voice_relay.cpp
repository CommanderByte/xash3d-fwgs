#include "engine/server/server_voice_relay.hpp"
#include "engine/server/server_message_envelope.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool ListenerMaskIncludes(std::uint32_t listenerMask, int recipientIndex)
{
	if (recipientIndex < 0 || recipientIndex >= 32)
		return false;

	return (listenerMask & (1U << recipientIndex)) != 0;
}

}

bool IsVoicePayloadTooLarge(unsigned int payloadSize)
{
	return payloadSize > kVoiceRelayMaxIncomingPayloadBytes;
}

VoiceRelayDecision BuildVoiceInputGateDecision(
	const VoiceInputGateRequest &request)
{
	VoiceRelayDecision decision = {};
	decision.shouldRelay = false;
	decision.reason = VoiceRelayStopReason::None;

	if (!request.voiceEnabled)
	{
		decision.reason = VoiceRelayStopReason::VoiceDisabled;
		return decision;
	}

	if (!request.senderSpawned)
	{
		decision.reason = VoiceRelayStopReason::SenderNotSpawned;
		return decision;
	}

	decision.shouldRelay = true;
	return decision;
}

VoiceRelayDecision BuildVoicePostPhysicsGateDecision(
	const VoicePostPhysicsGateRequest &request)
{
	VoiceRelayDecision decision = {};
	decision.shouldRelay = false;
	decision.reason = VoiceRelayStopReason::None;

	if (request.physicsHandled)
	{
		decision.reason = VoiceRelayStopReason::PhysicsHandled;
		return decision;
	}

	if (request.maxClients <= 1 && !request.voiceSingleplayer)
	{
		decision.reason = VoiceRelayStopReason::SinglePlayerSuppressed;
		return decision;
	}

	decision.shouldRelay = true;
	return decision;
}

VoiceRecipientDecision BuildVoiceRecipientDecision(
	const VoiceRecipientRequest &request)
{
	VoiceRecipientDecision decision = {};
	const bool local = request.senderIndex == request.recipientIndex;

	if (!local)
	{
		if (!request.recipientConnected)
		{
			decision.reason = VoiceRecipientSkipReason::RecipientNotConnected;
			return decision;
		}

		if (!ListenerMaskIncludes(request.listenerMask, request.recipientIndex))
		{
			decision.reason = VoiceRecipientSkipReason::ListenerDisabled;
			return decision;
		}
	}

	// Preserve the legacy capacity check: it uses the original incoming size
	// before the local no-loopback payload is collapsed to zero bytes.
	const unsigned int requiredBytes =
		request.payloadSize + static_cast<unsigned int>(kVoiceRelayDatagramCapacityOverheadBytes);
	if (request.datagramBytesLeft < 0 ||
		static_cast<unsigned int>(request.datagramBytesLeft) < requiredBytes)
	{
		decision.reason = VoiceRecipientSkipReason::DatagramTooSmall;
		return decision;
	}

	decision.shouldSend = true;
	decision.reason = VoiceRecipientSkipReason::None;
	decision.outgoingPayloadSize =
		(local && !request.loopbackRequested) ? 0U : request.payloadSize;
	return decision;
}

void WriteVoiceDataPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	int senderIndex,
	unsigned int frames,
	const void *payload,
	unsigned int payloadSize)
{
	WriteServerMessageByte(buffer, static_cast<unsigned int>(senderIndex));
	WriteServerMessageByte(buffer, frames);
	buffer.writeSigned(static_cast<int>(payloadSize), 16);

	if (payloadSize == 0)
		return;

	buffer.writeBits(payload, payloadSize << 3);
}

void WriteVoiceDataMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	int senderIndex,
	unsigned int frames,
	const void *payload,
	unsigned int payloadSize)
{
	WriteServerMessageCommand(buffer, kVoiceRelayServiceCommand);
	WriteVoiceDataPayload(buffer, senderIndex, frames, payload, payloadSize);
}

}
}
}
