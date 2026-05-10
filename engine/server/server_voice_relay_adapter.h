#ifndef XASH_ENGINE_SERVER_SERVER_VOICE_RELAY_ADAPTER_H
#define XASH_ENGINE_SERVER_SERVER_VOICE_RELAY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_voice_relay_decision_s
{
	int should_relay;
	int reason;
} sv_voice_relay_decision_t;

typedef struct sv_voice_recipient_decision_s
{
	int should_send;
	unsigned int outgoing_payload_size;
	int reason;
} sv_voice_recipient_decision_t;

typedef struct sv_voice_relay_write_result_s
{
	int current_bit;
	int overflow;
} sv_voice_relay_write_result_t;

int SV_VoiceRelay_IsPayloadTooLarge(unsigned int payload_size);

sv_voice_relay_decision_t SV_VoiceRelay_BuildInputGateDecision(
	int voice_enabled,
	int sender_spawned);

sv_voice_relay_decision_t SV_VoiceRelay_BuildPostPhysicsGateDecision(
	int physics_handled,
	int max_clients,
	int voice_singleplayer);

sv_voice_recipient_decision_t SV_VoiceRelay_BuildRecipientDecision(
	int sender_index,
	int recipient_index,
	int recipient_connected,
	unsigned int listener_mask,
	int loopback_requested,
	unsigned int payload_size,
	int datagram_bytes_left);

sv_voice_relay_write_result_t SV_VoiceRelay_WritePayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int sender_index,
	unsigned int frames,
	const void *payload,
	unsigned int payload_size);

#ifdef __cplusplus
}
#endif

#endif
