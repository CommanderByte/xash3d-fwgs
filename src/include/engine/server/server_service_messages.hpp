#ifndef XASH_ENGINE_SERVER_SERVER_SERVICE_MESSAGES_HPP
#define XASH_ENGINE_SERVER_SERVER_SERVICE_MESSAGES_HPP

#include "engine/network/network_buffer.hpp"

#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr std::uint8_t kServiceMessageSetView = 5;
constexpr std::uint8_t kServiceMessageStuffText = 9;
constexpr std::uint8_t kServiceMessageSetPause = 24;
constexpr std::uint8_t kServiceMessageFileTransferFailed = 49;
constexpr std::uint8_t kServiceMessageVoiceInit = 52;

constexpr const char *kServiceMessageReconnectCommand = "reconnect\n";
constexpr const char *kServiceMessageDefaultVoiceCodec = "opus_custom_44k_512";

void WriteServiceCommand(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint8_t command);

void WriteFileTransferFailedPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *filename);
void WriteFileTransferFailedMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *filename);

void WriteReconnectPayload(
	xash::engine::network::NetworkBitBuffer &buffer);
void WriteReconnectMessage(
	xash::engine::network::NetworkBitBuffer &buffer);

void WriteSetViewPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	int entityIndex);
void WriteSetViewMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	int entityIndex);

void WriteSetPausePayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	bool paused);
void WriteSetPauseMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	bool paused);

void WriteVoiceInitPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *codec,
	int quality);
void WriteVoiceInitMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *codec,
	int quality);

}
}
}

#endif
