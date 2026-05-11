#include "engine/server/messaging/server_message_envelope.hpp"

#include <cstddef>
#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

constexpr int kMessageClientStateSpawned = 4;

std::uint32_t ListenerMaskForRecipient(bool listenerEnabled, int recipientIndex)
{
	if (!listenerEnabled || recipientIndex < 0 || recipientIndex >= 32)
		return 0U;

	return 1U << recipientIndex;
}

}

void WriteServerMessageByte(
	xash::engine::network::NetworkBitBuffer &buffer,
	unsigned int value)
{
	buffer.writeUnsigned(static_cast<std::uint8_t>(value), 8);
}

void WriteServerMessageCommand(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint8_t command)
{
	WriteServerMessageByte(buffer, command);
}

void WriteServerMessageString(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *value)
{
	if (!value)
		value = "";

	const std::size_t length = std::strlen(value);
	for (std::size_t i = 0; i <= length; ++i)
		WriteServerMessageByte(buffer, static_cast<unsigned char>(value[i]));
}

void WriteServerMessageBytes(
	xash::engine::network::NetworkBitBuffer &buffer,
	const std::uint8_t *data,
	std::size_t size)
{
	for (std::size_t i = 0; i < size; ++i)
		WriteServerMessageByte(buffer, data ? data[i] : 0);
}

ServerMessageEnvelope BuildServerMessageEnvelope(
	std::uint8_t command,
	ServerMessageDeliveryClass deliveryClass)
{
	ServerMessageEnvelope envelope = {};
	envelope.command = command;
	envelope.deliveryClass = deliveryClass;
	return envelope;
}

ServerMessageRecipientFacts DefaultServerMessageRecipientFacts()
{
	ServerMessageRecipientFacts facts = {};
	facts.clientState = kMessageClientStateSpawned;
	facts.reliable = false;
	facts.userMessage = false;
	facts.spectatorProxyMode = false;
	facts.clientIsSpectatorProxy = false;
	facts.hasEdict = true;
	facts.fakeClient = false;
	facts.predictionFiltered = false;
	facts.groupPasses = true;
	facts.visible = true;
	facts.invokerValid = true;
	facts.notHost = false;
	facts.hostOnly = false;
	facts.localWeapons = false;
	facts.currentClient = false;
	facts.invokerClient = false;
	facts.recipientConnected = true;
	facts.listenerEnabled = true;
	facts.loopbackRequested = true;
	facts.voicePayloadSize = 32;
	facts.datagramBytesLeft = 64;
	return facts;
}

MulticastRecipientRequest BuildMulticastRecipientRequest(
	const ServerMessageRecipientFacts &facts)
{
	MulticastRecipientRequest request = {};
	request.clientState = facts.clientState;
	request.reliable = facts.reliable;
	request.userMessage = facts.userMessage;
	request.spectatorProxyMode = facts.spectatorProxyMode;
	request.clientIsSpectatorProxy = facts.clientIsSpectatorProxy;
	request.hasEdict = facts.hasEdict;
	request.fakeClient = facts.fakeClient;
	request.predictionFiltered = facts.predictionFiltered;
	request.groupPasses = facts.groupPasses;
	request.visible = facts.visible;
	return request;
}

ServerEventRecipientInput BuildServerEventRecipientInput(
	const ServerMessageRecipientFacts &facts)
{
	ServerEventRecipientInput input = {};
	input.spawned = facts.clientState >= kMessageClientStateSpawned;
	input.hasEdict = facts.hasEdict;
	input.fakeClient = facts.fakeClient;
	input.invokerValid = facts.invokerValid;
	input.groupPasses = facts.groupPasses;
	input.visible = facts.visible;
	input.notHost = facts.notHost;
	input.hostOnly = facts.hostOnly;
	input.localWeapons = facts.localWeapons;
	input.currentClient = facts.currentClient;
	input.invokerClient = facts.invokerClient;
	return input;
}

VoiceRecipientRequest BuildVoiceRecipientRequest(
	const ServerMessageRecipientFacts &facts,
	int senderIndex,
	int recipientIndex)
{
	VoiceRecipientRequest request = {};
	request.senderIndex = senderIndex;
	request.recipientIndex = recipientIndex;
	request.recipientConnected = facts.recipientConnected;
	request.listenerMask = ListenerMaskForRecipient(
		facts.listenerEnabled,
		recipientIndex);
	request.loopbackRequested = facts.loopbackRequested;
	request.payloadSize = facts.voicePayloadSize;
	request.datagramBytesLeft = facts.datagramBytesLeft;
	return request;
}

}
}
}
