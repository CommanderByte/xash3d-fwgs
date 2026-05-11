#include "engine/server/messaging/server_service_messages.hpp"
#include "engine/server/messaging/server_message_envelope.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

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
	WriteServerMessageCommand(buffer, command);
}

void WriteFileTransferFailedPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *filename)
{
	WriteServerMessageString(buffer, filename);
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
	WriteServerMessageString(buffer, kServiceMessageReconnectCommand);
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
	WriteServerMessageString(buffer, VoiceCodecOrDefault(codec));
	WriteServerMessageByte(buffer, static_cast<unsigned int>(quality));
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
