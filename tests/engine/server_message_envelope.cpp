#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/messaging/server_customization_message.hpp"
#include "engine/server/messaging/server_frame_datagram.hpp"
#include "engine/server/messaging/server_message_envelope.hpp"
#include "engine/server/messaging/server_packet_entities_delta.hpp"
#include "engine/server/messaging/server_resource_message.hpp"
#include "engine/server/messaging/server_service_messages.hpp"
#include "engine/server/messaging/server_sound_message.hpp"
#include "engine/server/messaging/server_spawn_handshake.hpp"
#include "engine/server/messaging/server_static_messages.hpp"
#include "engine/server/messaging/server_text_messages.hpp"
#include "engine/server/messaging/server_userinfo_message.hpp"
#include "engine/server/messaging/server_voice_relay.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

namespace
{

constexpr std::uint8_t kRepresentativeUserinfoCommand = 13;
constexpr std::uint8_t kRepresentativeResourceListCommand = 43;
constexpr std::uint8_t kRepresentativeCustomizationCommand = 46;

void WriteRepresentativePrint(NetworkBitBuffer &buffer)
{
	WritePrintMessage(buffer, "hello\n");
}

void WriteRepresentativeService(NetworkBitBuffer &buffer)
{
	WriteSetViewMessage(buffer, 513);
}

void WriteRepresentativeSound(NetworkBitBuffer &buffer)
{
	SoundMessagePayload payload = {};
	payload.soundIndex = 42;
	payload.channel = 2;
	payload.volume = kSoundMessageNormalVolume;
	payload.attenuation = kSoundMessageNoAttenuation;
	payload.pitch = kSoundMessageNormalPitch;
	payload.entityIndex = 7;
	payload.largeCoordinates = false;
	WriteSoundMessage(buffer, payload);
}

void WriteRepresentativeStatic(NetworkBitBuffer &buffer)
{
	StaticDecalPayload payload = {};
	payload.origin[0] = 1.0f;
	payload.origin[1] = -2.0f;
	payload.origin[2] = 3.0f;
	payload.decalIndex = 12;
	payload.entityIndex = 0;
	payload.flags = 5;
	payload.scale = 1.0f;
	WriteBspDecalMessage(buffer, payload);
}

void WriteRepresentativeVoice(NetworkBitBuffer &buffer)
{
	static const unsigned char payload[] = {0xAA, 0xBB};
	WriteVoiceDataMessage(buffer, 3, 4, payload, sizeof(payload));
}

void WriteRepresentativeSignon(NetworkBitBuffer &buffer)
{
	WriteSignonNumberMessage(buffer, 1);
}

void WriteRepresentativeUserinfo(NetworkBitBuffer &buffer)
{
	WriteServerMessageCommand(buffer, kRepresentativeUserinfoCommand);

	UserinfoUpdatePayload payload = {};
	payload.clientIndex = 2;
	payload.userId = 17;
	payload.active = true;
	payload.userinfo = "\\name\\barney";
	payload.hashedCdKeyDigest = nullptr;
	WriteUserinfoUpdatePayload(buffer, payload);
}

void WriteRepresentativeResourceList(NetworkBitBuffer &buffer)
{
	WriteServerMessageCommand(buffer, kRepresentativeResourceListCommand);

	ResourceMessageRow row = {};
	row.type = ResourceType::Model;
	row.name = "models/w_9mmhandgun.mdl";
	row.index = 9;
	row.downloadSize = 2048;
	row.flags = kResourceMessageFlagFatalIfMissing;
	WriteResourceMessageRow(buffer, row);
}

void WriteRepresentativeCustomization(NetworkBitBuffer &buffer)
{
	WriteServerMessageCommand(buffer, kRepresentativeCustomizationCommand);

	CustomizationMessage message = {};
	message.playerNumber = 1;
	message.type = ResourceType::Decal;
	message.name = "custom.hpk";
	message.index = 5;
	message.downloadSize = 128;
	message.flags = 0;
	WriteCustomizationMessagePayload(buffer, message);
}

bool MessageCommandMatches(
	void (*writeMessage)(NetworkBitBuffer &buffer),
	std::uint8_t expectedCommand)
{
	unsigned char data[128] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	writeMessage(writer);

	NetworkBitBuffer reader(data, writer.tellBit());
	return reader.readUnsigned(8) == expectedCommand &&
		writer.tellBit() > 8 &&
		reader.tellBit() == 8 &&
		!reader.overflow() &&
		!writer.overflow();
}

bool TestRepresentativeMessageEnvelopes()
{
	return MessageCommandMatches(WriteRepresentativePrint, kTextMessagePrint) &&
		MessageCommandMatches(WriteRepresentativeService, kServiceMessageSetView) &&
		MessageCommandMatches(WriteRepresentativeSound, kSoundMessageCommand) &&
		MessageCommandMatches(WriteRepresentativeStatic, kStaticMessageBspDecalCommand) &&
		MessageCommandMatches(WriteRepresentativeVoice, kVoiceRelayServiceCommand) &&
		MessageCommandMatches(
			WriteRepresentativeSignon,
			kServerdataSignonNumberCommand) &&
		MessageCommandMatches(
			WriteRepresentativeUserinfo,
			kRepresentativeUserinfoCommand) &&
		MessageCommandMatches(
			WriteRepresentativeResourceList,
			kRepresentativeResourceListCommand) &&
		MessageCommandMatches(
			WriteRepresentativeCustomization,
			kRepresentativeCustomizationCommand);
}

bool TestEnvelopeCommandAndStringHelpers()
{
	static const unsigned char expected[] =
	{
		kTextMessageStuffText, 'r', 'e', 'c', 'o', 'n',
		'n', 'e', 'c', 't', '\n', '\0',
	};

	unsigned char data[32] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteServerMessageCommand(writer, kTextMessageStuffText);
	WriteServerMessageString(writer, "reconnect\n");

	const ServerMessageEnvelope envelope =
		BuildServerMessageEnvelope(
			kTextMessageStuffText,
			ServerMessageDeliveryClass::ServerReliable);

	return envelope.command == kTextMessageStuffText &&
		envelope.deliveryClass == ServerMessageDeliveryClass::ServerReliable &&
		writer.tellBit() == sizeof(expected) * 8 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

bool TestEnvelopeHelperOverflow()
{
	unsigned char data[4] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteServerMessageCommand(writer, kTextMessagePrint);
	WriteServerMessageString(writer, "too-long");
	return writer.overflow();
}

bool TestRecipientFactsAllowDefaultRecipient()
{
	const ServerMessageRecipientFacts facts =
		DefaultServerMessageRecipientFacts();

	const MulticastRecipientDecision multicast =
		BuildMulticastRecipientDecision(
			BuildMulticastRecipientRequest(facts));
	const ServerEventRecipientDecision event =
		BuildServerEventRecipientDecision(
			BuildServerEventRecipientInput(facts));
	const VoiceRecipientDecision voice =
		BuildVoiceRecipientDecision(
			BuildVoiceRecipientRequest(facts, 1, 2));

	return multicast.shouldSend &&
		multicast.route == MulticastRecipientRoute::ClientDatagram &&
		event.deliver &&
		voice.shouldSend &&
		voice.outgoingPayloadSize == facts.voicePayloadSize;
}

bool TestRecipientFactsPreservePolicyDifferences()
{
	ServerMessageRecipientFacts facts =
		DefaultServerMessageRecipientFacts();
	facts.visible = false;

	const MulticastRecipientDecision multicast =
		BuildMulticastRecipientDecision(
			BuildMulticastRecipientRequest(facts));
	const ServerEventRecipientDecision event =
		BuildServerEventRecipientDecision(
			BuildServerEventRecipientInput(facts));
	const VoiceRecipientDecision voice =
		BuildVoiceRecipientDecision(
			BuildVoiceRecipientRequest(facts, 1, 2));

	return !multicast.shouldSend &&
		multicast.reason == MulticastRecipientSkipReason::NotVisible &&
		!event.deliver &&
		event.rejectReason == ServerEventRecipientRejectReason::NotVisible &&
		voice.shouldSend;
}

bool TestRecipientFactsRejectUnspawnedRecipient()
{
	ServerMessageRecipientFacts facts =
		DefaultServerMessageRecipientFacts();
	facts.clientState = 2;

	const MulticastRecipientDecision multicast =
		BuildMulticastRecipientDecision(
			BuildMulticastRecipientRequest(facts));
	const ServerEventRecipientDecision event =
		BuildServerEventRecipientDecision(
			BuildServerEventRecipientInput(facts));

	return !multicast.shouldSend &&
		multicast.reason == MulticastRecipientSkipReason::NeedsSpawnedClient &&
		!event.deliver &&
		event.rejectReason == ServerEventRecipientRejectReason::NotSpawned;
}

bool TestVoiceRecipientFactsUseListenerMask()
{
	ServerMessageRecipientFacts facts =
		DefaultServerMessageRecipientFacts();
	facts.listenerEnabled = false;

	const VoiceRecipientDecision remote =
		BuildVoiceRecipientDecision(
			BuildVoiceRecipientRequest(facts, 1, 2));

	facts.listenerEnabled = false;
	facts.loopbackRequested = false;
	const VoiceRecipientDecision local =
		BuildVoiceRecipientDecision(
			BuildVoiceRecipientRequest(facts, 1, 1));

	return !remote.shouldSend &&
		remote.reason == VoiceRecipientSkipReason::ListenerDisabled &&
		local.shouldSend &&
		local.outgoingPayloadSize == 0;
}

bool TestFrameAndPacketFactsStayInMessagingDomain()
{
	const FrameTransferPlan datagram =
		BuildServerUnreliableDatagramPlan(15, 16);
	const FrameReliableResendPlan resend =
		BuildReliableResendPlan(
			kFrameResendUserinfoFlag | kFrameResendMovevarsFlag,
			9.0,
			10.0,
			64,
			8);
	const PacketEntityHeaderPlan header =
		BuildPacketEntityHeaderPlan(true, 193, 256, 64, 12);
	const PacketEntityCursorPlan cursor =
		BuildPacketEntityCursorPlan(1, 3, 42, 2, 4, 42, 99999);

	return datagram.action == FrameTransferAction::Copy &&
		resend.sendUserinfo &&
		resend.sendMovevars &&
		header.action == PacketEntityHeaderAction::Delta &&
		header.usePreviousFrame &&
		cursor.action == PacketEntityCursorAction::DeltaFromOld &&
		cursor.advanceNew == 1 &&
		cursor.advanceOld == 1;
}

}

int main()
{
	if (!TestRepresentativeMessageEnvelopes() ||
		!TestEnvelopeCommandAndStringHelpers() ||
		!TestEnvelopeHelperOverflow() ||
		!TestRecipientFactsAllowDefaultRecipient() ||
		!TestRecipientFactsPreservePolicyDifferences() ||
		!TestRecipientFactsRejectUnspawnedRecipient() ||
		!TestVoiceRecipientFactsUseListenerMask() ||
		!TestFrameAndPacketFactsStayInMessagingDomain())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
