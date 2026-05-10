#include "engine/server/server_service_messages.hpp"

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

void WriteByte(
	xash::engine::network::NetworkBitBuffer &buffer,
	unsigned int value)
{
	buffer.writeUnsigned(static_cast<std::uint8_t>(value), 8);
}

void WriteString(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *value)
{
	if (!value)
		value = "";

	const std::size_t length = std::strlen(value);
	for (std::size_t i = 0; i <= length; ++i)
		WriteByte(buffer, static_cast<unsigned char>(value[i]));
}

const char *VoiceCodecOrDefault(const char *codec)
{
	if (!codec || codec[0] == '\0')
		return kServiceMessageDefaultVoiceCodec;

	return codec;
}

}

void WriteServiceCommand(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint8_t command)
{
	WriteByte(buffer, command);
}

void WriteFileTransferFailedPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *filename)
{
	WriteString(buffer, filename);
}

void WriteFileTransferFailedMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *filename)
{
	WriteServiceCommand(buffer, kServiceMessageFileTransferFailed);
	WriteFileTransferFailedPayload(buffer, filename);
}

void WriteReconnectPayload(
	xash::engine::network::NetworkBitBuffer &buffer)
{
	WriteString(buffer, kServiceMessageReconnectCommand);
}

void WriteReconnectMessage(
	xash::engine::network::NetworkBitBuffer &buffer)
{
	WriteServiceCommand(buffer, kServiceMessageStuffText);
	WriteReconnectPayload(buffer);
}

void WriteSetViewPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	int entityIndex)
{
	buffer.writeUnsigned(static_cast<std::uint16_t>(entityIndex), 16);
}

void WriteSetViewMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	int entityIndex)
{
	WriteServiceCommand(buffer, kServiceMessageSetView);
	WriteSetViewPayload(buffer, entityIndex);
}

void WriteSetPausePayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	bool paused)
{
	buffer.writeOneBit(paused ? 1 : 0);
}

void WriteSetPauseMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	bool paused)
{
	WriteServiceCommand(buffer, kServiceMessageSetPause);
	WriteSetPausePayload(buffer, paused);
}

void WriteVoiceInitPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *codec,
	int quality)
{
	WriteString(buffer, VoiceCodecOrDefault(codec));
	WriteByte(buffer, static_cast<unsigned int>(quality));
}

void WriteVoiceInitMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *codec,
	int quality)
{
	WriteServiceCommand(buffer, kServiceMessageVoiceInit);
	WriteVoiceInitPayload(buffer, codec, quality);
}

}
}
}
