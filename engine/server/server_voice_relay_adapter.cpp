#include "server_voice_relay_adapter.h"

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_voice_relay.hpp"

#include <cstddef>

namespace
{

xash::engine::network::NetworkBitBuffer MakeBuffer(
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	return xash::engine::network::NetworkBitBuffer(
		data,
		data_bits < 0 ? 0U : static_cast<std::size_t>(data_bits),
		current_bit < 0 ? 0U : static_cast<std::size_t>(current_bit));
}

sv_voice_relay_write_result_t MakeResult(
	const xash::engine::network::NetworkBitBuffer &buffer)
{
	sv_voice_relay_write_result_t result = {};
	result.current_bit = static_cast<int>(buffer.tellBit());
	result.overflow = buffer.overflow() ? 1 : 0;
	return result;
}

}

extern "C" int SV_VoiceRelay_IsPayloadTooLarge(unsigned int payload_size)
{
	return xash::engine::server::IsVoicePayloadTooLarge(payload_size) ? 1 : 0;
}

extern "C" sv_voice_relay_decision_t SV_VoiceRelay_BuildInputGateDecision(
	int voice_enabled,
	int sender_spawned)
{
	xash::engine::server::VoiceInputGateRequest request = {};
	request.voiceEnabled = voice_enabled != 0;
	request.senderSpawned = sender_spawned != 0;

	const xash::engine::server::VoiceRelayDecision modern =
		xash::engine::server::BuildVoiceInputGateDecision(request);

	sv_voice_relay_decision_t legacy = {};
	legacy.should_relay = modern.shouldRelay ? 1 : 0;
	legacy.reason = static_cast<int>(modern.reason);
	return legacy;
}

extern "C" sv_voice_relay_decision_t SV_VoiceRelay_BuildPostPhysicsGateDecision(
	int physics_handled,
	int max_clients,
	int voice_singleplayer)
{
	xash::engine::server::VoicePostPhysicsGateRequest request = {};
	request.physicsHandled = physics_handled != 0;
	request.maxClients = max_clients;
	request.voiceSingleplayer = voice_singleplayer != 0;

	const xash::engine::server::VoiceRelayDecision modern =
		xash::engine::server::BuildVoicePostPhysicsGateDecision(request);

	sv_voice_relay_decision_t legacy = {};
	legacy.should_relay = modern.shouldRelay ? 1 : 0;
	legacy.reason = static_cast<int>(modern.reason);
	return legacy;
}

extern "C" sv_voice_recipient_decision_t SV_VoiceRelay_BuildRecipientDecision(
	int sender_index,
	int recipient_index,
	int recipient_connected,
	unsigned int listener_mask,
	int loopback_requested,
	unsigned int payload_size,
	int datagram_bytes_left)
{
	xash::engine::server::VoiceRecipientRequest request = {};
	request.senderIndex = sender_index;
	request.recipientIndex = recipient_index;
	request.recipientConnected = recipient_connected != 0;
	request.listenerMask = listener_mask;
	request.loopbackRequested = loopback_requested != 0;
	request.payloadSize = payload_size;
	request.datagramBytesLeft = datagram_bytes_left;

	const xash::engine::server::VoiceRecipientDecision modern =
		xash::engine::server::BuildVoiceRecipientDecision(request);

	sv_voice_recipient_decision_t legacy = {};
	legacy.should_send = modern.shouldSend ? 1 : 0;
	legacy.outgoing_payload_size = modern.outgoingPayloadSize;
	legacy.reason = static_cast<int>(modern.reason);
	return legacy;
}

extern "C" sv_voice_relay_write_result_t SV_VoiceRelay_WritePayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int sender_index,
	unsigned int frames,
	const void *payload,
	unsigned int payload_size)
{
	xash::engine::network::NetworkBitBuffer buffer =
		MakeBuffer(data, data_bits, current_bit);

	xash::engine::server::WriteVoiceDataPayload(
		buffer,
		sender_index,
		frames,
		payload,
		payload_size);

	return MakeResult(buffer);
}
