#ifndef XASH_ENGINE_SERVER_SERVER_MESSAGE_ENVELOPE_HPP
#define XASH_ENGINE_SERVER_SERVER_MESSAGE_ENVELOPE_HPP

#include "engine/network/network_buffer.hpp"
#include "engine/server/messaging/server_event_playback_policy.hpp"
#include "engine/server/messaging/server_multicast_policy.hpp"
#include "engine/server/messaging/server_voice_relay.hpp"

#include <cstddef>
#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

enum class ServerMessageDeliveryClass
{
	Unknown,
	ClientReliable,
	ClientDatagram,
	ServerReliable,
	Signon,
	SpectatorDatagram,
	MulticastScratch,
};

struct ServerMessageEnvelope
{
	std::uint8_t command;
	ServerMessageDeliveryClass deliveryClass;
};

struct ServerMessageRecipientFacts
{
	int clientState;
	bool reliable;
	bool userMessage;
	bool spectatorProxyMode;
	bool clientIsSpectatorProxy;
	bool hasEdict;
	bool fakeClient;
	bool predictionFiltered;
	bool groupPasses;
	bool visible;
	bool invokerValid;
	bool notHost;
	bool hostOnly;
	bool localWeapons;
	bool currentClient;
	bool invokerClient;
	bool recipientConnected;
	bool listenerEnabled;
	bool loopbackRequested;
	unsigned int voicePayloadSize;
	int datagramBytesLeft;
};

void WriteServerMessageByte(
	xash::engine::network::NetworkBitBuffer &buffer,
	unsigned int value);
void WriteServerMessageCommand(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint8_t command);
void WriteServerMessageString(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *value);
void WriteServerMessageBytes(
	xash::engine::network::NetworkBitBuffer &buffer,
	const std::uint8_t *data,
	std::size_t size);

ServerMessageEnvelope BuildServerMessageEnvelope(
	std::uint8_t command,
	ServerMessageDeliveryClass deliveryClass);

ServerMessageRecipientFacts DefaultServerMessageRecipientFacts();

MulticastRecipientRequest BuildMulticastRecipientRequest(
	const ServerMessageRecipientFacts &facts);
ServerEventRecipientInput BuildServerEventRecipientInput(
	const ServerMessageRecipientFacts &facts);
VoiceRecipientRequest BuildVoiceRecipientRequest(
	const ServerMessageRecipientFacts &facts,
	int senderIndex,
	int recipientIndex);

}
}
}

#endif
