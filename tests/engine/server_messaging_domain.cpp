#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/messaging/server_event_playback_policy.hpp"
#include "engine/server/messaging/server_frame_datagram.hpp"
#include "engine/server/messaging/server_message_envelope.hpp"
#include "engine/server/messaging/server_multicast_policy.hpp"
#include "engine/server/messaging/server_packet_entities_delta.hpp"
#include "engine/server/messaging/server_service_messages.hpp"
#include "engine/server/messaging/server_spawn_handshake.hpp"
#include "engine/server/messaging/server_text_messages.hpp"
#include "engine/server/messaging/server_voice_relay.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

namespace
{

MulticastDestinationRequest DestinationRequest(int destination)
{
	MulticastDestinationRequest request = {};
	request.destination = destination;
	request.serverLoading = false;
	request.originProvided = true;
	return request;
}

bool ReadStringEquals(NetworkBitBuffer &reader, const char *expected)
{
	for (std::size_t i = 0; ; ++i)
	{
		const unsigned int actual = reader.readUnsigned(8);
		if (actual != static_cast<unsigned char>(expected[i]))
			return false;

		if (expected[i] == '\0')
			return !reader.overflow();
	}
}

bool TestReliableEnvelopeRecipientAndRouteCompose()
{
	MulticastDestinationRequest destination =
		DestinationRequest(kMulticastDestinationOne);
	const MulticastDestinationPlan destinationPlan =
		BuildMulticastDestinationPlan(destination);

	ServerMessageRecipientFacts facts =
		DefaultServerMessageRecipientFacts();
	facts.reliable = destinationPlan.reliable;
	facts.userMessage = false;

	const MulticastRecipientDecision recipient =
		BuildMulticastRecipientDecision(
			BuildMulticastRecipientRequest(facts));
	const ServerEventRecipientDecision event =
		BuildServerEventRecipientDecision(
			BuildServerEventRecipientInput(facts));
	const ServerMessageEnvelope envelope =
		BuildServerMessageEnvelope(
			kTextMessagePrint,
			ServerMessageDeliveryClass::ClientReliable);

	return destinationPlan.action == MulticastDestinationAction::SendToClients &&
		destinationPlan.singleClient &&
		destinationPlan.reliable &&
		recipient.shouldSend &&
		recipient.route == MulticastRecipientRoute::ClientReliable &&
		event.deliver &&
		envelope.command == kTextMessagePrint &&
		envelope.deliveryClass == ServerMessageDeliveryClass::ClientReliable;
}

bool TestSpectatorEnvelopeRecipientAndRouteCompose()
{
	const MulticastDestinationPlan destinationPlan =
		BuildMulticastDestinationPlan(
			DestinationRequest(kMulticastDestinationSpectator));

	ServerMessageRecipientFacts facts =
		DefaultServerMessageRecipientFacts();
	facts.reliable = destinationPlan.reliable;
	facts.spectatorProxyMode = destinationPlan.spectatorProxy;
	facts.clientIsSpectatorProxy = true;

	const MulticastRecipientDecision recipient =
		BuildMulticastRecipientDecision(
			BuildMulticastRecipientRequest(facts));
	const ServerMessageEnvelope envelope =
		BuildServerMessageEnvelope(
			kTextMessagePrint,
			ServerMessageDeliveryClass::SpectatorDatagram);

	return destinationPlan.action == MulticastDestinationAction::SendToClients &&
		destinationPlan.spectatorProxy &&
		recipient.shouldSend &&
		recipient.route == MulticastRecipientRoute::SpectatorDatagram &&
		envelope.deliveryClass == ServerMessageDeliveryClass::SpectatorDatagram;
}

bool TestSharedFactsPreserveDifferentRecipientPolicies()
{
	ServerMessageRecipientFacts invisibleFacts =
		DefaultServerMessageRecipientFacts();
	invisibleFacts.visible = false;

	const MulticastRecipientDecision multicast =
		BuildMulticastRecipientDecision(
			BuildMulticastRecipientRequest(invisibleFacts));
	const ServerEventRecipientDecision event =
		BuildServerEventRecipientDecision(
			BuildServerEventRecipientInput(invisibleFacts));
	const VoiceRecipientDecision voice =
		BuildVoiceRecipientDecision(
			BuildVoiceRecipientRequest(invisibleFacts, 1, 2));

	ServerMessageRecipientFacts silentFacts =
		DefaultServerMessageRecipientFacts();
	silentFacts.listenerEnabled = false;

	const MulticastRecipientDecision silentMulticast =
		BuildMulticastRecipientDecision(
			BuildMulticastRecipientRequest(silentFacts));
	const ServerEventRecipientDecision silentEvent =
		BuildServerEventRecipientDecision(
			BuildServerEventRecipientInput(silentFacts));
	const VoiceRecipientDecision silentVoice =
		BuildVoiceRecipientDecision(
			BuildVoiceRecipientRequest(silentFacts, 1, 2));

	return !multicast.shouldSend &&
		multicast.reason == MulticastRecipientSkipReason::NotVisible &&
		!event.deliver &&
		event.rejectReason == ServerEventRecipientRejectReason::NotVisible &&
		voice.shouldSend &&
		silentMulticast.shouldSend &&
		silentEvent.deliver &&
		!silentVoice.shouldSend &&
		silentVoice.reason == VoiceRecipientSkipReason::ListenerDisabled;
}

bool TestPayloadWritersShareLegacyScratchBufferShape()
{
	unsigned char data[256] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);

	const ServerMessageEnvelope envelope =
		BuildServerMessageEnvelope(
			kTextMessagePrint,
			ServerMessageDeliveryClass::ServerReliable);
	WritePrintMessage(writer, "aggregate\n");
	WriteSetPauseMessage(writer, true);
	WriteVoiceInitMessage(writer, nullptr, 7);

	NetworkBitBuffer reader(data, writer.tellBit());

	return envelope.deliveryClass == ServerMessageDeliveryClass::ServerReliable &&
		reader.readUnsigned(8) == kTextMessagePrint &&
		ReadStringEquals(reader, "aggregate\n") &&
		reader.readUnsigned(8) == kServiceMessageSetPause &&
		reader.readOneBit() == 1 &&
		reader.readUnsigned(8) == kServiceMessageVoiceInit &&
		ReadStringEquals(reader, kServiceMessageDefaultVoiceCodec) &&
		reader.readUnsigned(8) == 7 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

bool TestSignonEnvelopeKeepsBufferOwnershipExternal()
{
	MulticastDestinationRequest destination =
		DestinationRequest(kMulticastDestinationInit);
	destination.serverLoading = true;

	const MulticastDestinationPlan plan =
		BuildMulticastDestinationPlan(destination);
	const ServerMessageEnvelope envelope =
		BuildServerMessageEnvelope(
			kServerdataSignonNumberCommand,
			ServerMessageDeliveryClass::Signon);

	unsigned char data[16] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteSignonNumberMessage(writer, 2);

	NetworkBitBuffer reader(data, writer.tellBit());

	return plan.action == MulticastDestinationAction::WriteSignon &&
		plan.clearMulticast &&
		envelope.deliveryClass == ServerMessageDeliveryClass::Signon &&
		reader.readUnsigned(8) == kServerdataSignonNumberCommand &&
		reader.readUnsigned(8) == 2 &&
		reader.tellBit() == writer.tellBit() &&
		!writer.overflow();
}

bool TestEventAndFrameAdjacentPlansCompose()
{
	const unsigned int rawFlags =
		kServerEventFlagReliable |
		kServerEventFlagUpdate |
		kServerEventFlagNotHost |
		kServerEventFlagHostOnly;
	const ServerEventPlaybackFlagPlan flagPlan =
		BuildServerEventPlaybackFlagPlan(rawFlags, -1.0f, false);

	ServerMessageRecipientFacts facts =
		DefaultServerMessageRecipientFacts();
	facts.reliable = flagPlan.reliable;
	const ServerEventRecipientDecision recipient =
		BuildServerEventRecipientDecision(
			BuildServerEventRecipientInput(facts));

	const ServerEventQueueSlotSnapshot slots[] =
	{
		{ 11U, 7 },
		{ 0U, 0 },
	};
	const ServerEventQueueSlotPlan slot =
		SelectServerEventQueueSlot(slots, 2, 11U, 7, true);

	const FrameTransferPlan reliableDatagram =
		BuildServerReliableDatagramPlan(64, 64);
	const FrameReliableResendPlan resend =
		BuildReliableResendPlan(
			kFrameResendUserinfoFlag | kFrameResendMovevarsFlag,
			1.0,
			2.0,
			32,
			8);
	const PacketEntityHeaderPlan header =
		BuildPacketEntityHeaderPlan(true, 80, 100, 64, 5);
	const PacketEntityCursorPlan cursor =
		BuildPacketEntityCursorPlan(0, 1, 10, 0, 1, 12, 99999);

	return flagPlan.allowPlayback &&
		flagPlan.delay == 0.0f &&
		flagPlan.clearedNotHost &&
		flagPlan.clearedHostOnly &&
		flagPlan.reliable &&
		recipient.deliver &&
		slot.hasSlot &&
		slot.slot == 0 &&
		slot.reusedUpdateSlot &&
		reliableDatagram.action == FrameTransferAction::Fragment &&
		resend.sendUserinfo &&
		resend.sendMovevars &&
		header.action == PacketEntityHeaderAction::Delta &&
		header.usePreviousFrame &&
		cursor.action == PacketEntityCursorAction::AddFromBaseline &&
		cursor.advanceNew == 1 &&
		cursor.advanceOld == 0;
}

}

int main()
{
	if (!TestReliableEnvelopeRecipientAndRouteCompose() ||
		!TestSpectatorEnvelopeRecipientAndRouteCompose() ||
		!TestSharedFactsPreserveDifferentRecipientPolicies() ||
		!TestPayloadWritersShareLegacyScratchBufferShape() ||
		!TestSignonEnvelopeKeepsBufferOwnershipExternal() ||
		!TestEventAndFrameAdjacentPlansCompose())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
