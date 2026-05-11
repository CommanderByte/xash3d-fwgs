#include <cstdlib>
#include <cstddef>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/messaging/server_voice_relay.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

static VoiceInputGateRequest InputGateRequest()
{
	VoiceInputGateRequest request = {};
	request.voiceEnabled = true;
	request.senderSpawned = true;
	return request;
}

static VoicePostPhysicsGateRequest PostPhysicsGateRequest()
{
	VoicePostPhysicsGateRequest request = {};
	request.physicsHandled = false;
	request.maxClients = 4;
	request.voiceSingleplayer = false;
	return request;
}

static VoiceRecipientRequest RecipientRequest()
{
	VoiceRecipientRequest request = {};
	request.senderIndex = 1;
	request.recipientIndex = 2;
	request.recipientConnected = true;
	request.listenerMask = 1U << 2;
	request.loopbackRequested = true;
	request.payloadSize = 32;
	request.datagramBytesLeft = 64;
	return request;
}

static bool TestPayloadSizeLimit()
{
	return !IsVoicePayloadTooLarge(kVoiceRelayMaxIncomingPayloadBytes) &&
		IsVoicePayloadTooLarge(kVoiceRelayMaxIncomingPayloadBytes + 1);
}

static bool TestRelayGateAllowsNormalMultiplayer()
{
	const VoiceRelayDecision inputDecision =
		BuildVoiceInputGateDecision(InputGateRequest());
	const VoiceRelayDecision postPhysicsDecision =
		BuildVoicePostPhysicsGateDecision(PostPhysicsGateRequest());

	return inputDecision.shouldRelay &&
		inputDecision.reason == VoiceRelayStopReason::None &&
		postPhysicsDecision.shouldRelay &&
		postPhysicsDecision.reason == VoiceRelayStopReason::None;
}

static bool TestRelayGateBlocksDisabledVoice()
{
	VoiceInputGateRequest request = InputGateRequest();
	request.voiceEnabled = false;

	const VoiceRelayDecision decision =
		BuildVoiceInputGateDecision(request);

	return !decision.shouldRelay &&
		decision.reason == VoiceRelayStopReason::VoiceDisabled;
}

static bool TestRelayGateBlocksUnspawnedSender()
{
	VoiceInputGateRequest request = InputGateRequest();
	request.senderSpawned = false;

	const VoiceRelayDecision decision =
		BuildVoiceInputGateDecision(request);

	return !decision.shouldRelay &&
		decision.reason == VoiceRelayStopReason::SenderNotSpawned;
}

static bool TestRelayGateHonorsPhysicsHandler()
{
	VoicePostPhysicsGateRequest request = PostPhysicsGateRequest();
	request.physicsHandled = true;

	const VoiceRelayDecision decision =
		BuildVoicePostPhysicsGateDecision(request);

	return !decision.shouldRelay &&
		decision.reason == VoiceRelayStopReason::PhysicsHandled;
}

static bool TestRelayGateSuppressesSinglePlayerWhenDisabled()
{
	VoicePostPhysicsGateRequest request = PostPhysicsGateRequest();
	request.maxClients = 1;
	request.voiceSingleplayer = false;

	const VoiceRelayDecision decision =
		BuildVoicePostPhysicsGateDecision(request);

	return !decision.shouldRelay &&
		decision.reason == VoiceRelayStopReason::SinglePlayerSuppressed;
}

static bool TestRecipientAllowsListenedRemoteClient()
{
	const VoiceRecipientDecision decision =
		BuildVoiceRecipientDecision(RecipientRequest());

	return decision.shouldSend &&
		decision.outgoingPayloadSize == 32 &&
		decision.reason == VoiceRecipientSkipReason::None;
}

static bool TestRecipientBlocksDisconnectedRemoteClient()
{
	VoiceRecipientRequest request = RecipientRequest();
	request.recipientConnected = false;

	const VoiceRecipientDecision decision =
		BuildVoiceRecipientDecision(request);

	return !decision.shouldSend &&
		decision.reason == VoiceRecipientSkipReason::RecipientNotConnected;
}

static bool TestRecipientBlocksListenerMaskMiss()
{
	VoiceRecipientRequest request = RecipientRequest();
	request.listenerMask = 1U << 3;

	const VoiceRecipientDecision decision =
		BuildVoiceRecipientDecision(request);

	return !decision.shouldSend &&
		decision.reason == VoiceRecipientSkipReason::ListenerDisabled;
}

static bool TestLocalSenderWithoutLoopbackSendsEmptyPayload()
{
	VoiceRecipientRequest request = RecipientRequest();
	request.recipientIndex = request.senderIndex;
	request.recipientConnected = false;
	request.listenerMask = 0;
	request.loopbackRequested = false;

	const VoiceRecipientDecision decision =
		BuildVoiceRecipientDecision(request);

	return decision.shouldSend &&
		decision.outgoingPayloadSize == 0 &&
		decision.reason == VoiceRecipientSkipReason::None;
}

static bool TestLocalNoLoopbackStillUsesOriginalSizeForCapacity()
{
	VoiceRecipientRequest request = RecipientRequest();
	request.recipientIndex = request.senderIndex;
	request.loopbackRequested = false;
	request.payloadSize = 32;
	request.datagramBytesLeft = 6;

	const VoiceRecipientDecision decision =
		BuildVoiceRecipientDecision(request);

	return !decision.shouldSend &&
		decision.reason == VoiceRecipientSkipReason::DatagramTooSmall;
}

static bool TestVoiceDataMessageBytes()
{
	static const unsigned char payload[] = { 0xAA, 0xBB, 0xCC };
	static const unsigned char expected[] =
	{
		kVoiceRelayServiceCommand, 0x04, 0x07, 0x03, 0x00,
		0xAA, 0xBB, 0xCC,
	};

	unsigned char data[32] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteVoiceDataMessage(writer, 4, 7, payload, sizeof(payload));

	return writer.tellBit() == sizeof(expected) * 8 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

static bool TestVoicePayloadCanAppendAfterLegacyCommand()
{
	static const unsigned char payload[] = { 0x10, 0x20 };
	unsigned char data[16] = {};
	data[0] = kVoiceRelayServiceCommand;

	NetworkBitBuffer writer(data, sizeof(data) << 3, 8);
	WriteVoiceDataPayload(writer, 2, 5, payload, sizeof(payload));

	return writer.tellBit() == 8 + 6 * 8 &&
		data[0] == kVoiceRelayServiceCommand &&
		data[1] == 0x02 &&
		data[2] == 0x05 &&
		data[3] == 0x02 &&
		data[4] == 0x00 &&
		data[5] == 0x10 &&
		data[6] == 0x20 &&
		!writer.overflow();
}

static bool TestVoicePayloadOverflow()
{
	static const unsigned char payload[] = { 0xAA, 0xBB, 0xCC };
	unsigned char data[4] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteVoiceDataMessage(writer, 1, 1, payload, sizeof(payload));
	return writer.overflow();
}

int main()
{
	if (!TestPayloadSizeLimit() ||
		!TestRelayGateAllowsNormalMultiplayer() ||
		!TestRelayGateBlocksDisabledVoice() ||
		!TestRelayGateBlocksUnspawnedSender() ||
		!TestRelayGateHonorsPhysicsHandler() ||
		!TestRelayGateSuppressesSinglePlayerWhenDisabled() ||
		!TestRecipientAllowsListenedRemoteClient() ||
		!TestRecipientBlocksDisconnectedRemoteClient() ||
		!TestRecipientBlocksListenerMaskMiss() ||
		!TestLocalSenderWithoutLoopbackSendsEmptyPayload() ||
		!TestLocalNoLoopbackStillUsesOriginalSizeForCapacity() ||
		!TestVoiceDataMessageBytes() ||
		!TestVoicePayloadCanAppendAfterLegacyCommand() ||
		!TestVoicePayloadOverflow())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
