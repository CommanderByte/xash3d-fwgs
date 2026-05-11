#include <cstdlib>
#include <cstddef>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/messaging/server_service_messages.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

static bool ReadString(NetworkBitBuffer &reader, char *out, std::size_t outSize)
{
	if (!out || outSize == 0)
		return false;

	std::size_t length = 0;
	for (;;)
	{
		const char ch = static_cast<char>(reader.readUnsigned(8));
		if (length + 1 < outSize)
			out[length++] = ch;

		if (ch == '\0')
			break;
	}

	out[outSize - 1] = '\0';
	return !reader.overflow();
}

static bool TestFileTransferFailedMessage()
{
	static const unsigned char expected[] =
	{
		0x31, 'm', 'a', 'p', 's', '/', 'e', '1',
		'm', '1', '.', 'b', 's', 'p', 0x00,
	};

	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteFileTransferFailedMessage(writer, "maps/e1m1.bsp");

	return writer.tellBit() == sizeof(expected) * 8 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

static bool TestReconnectMessage()
{
	static const unsigned char expected[] =
	{
		0x09, 'r', 'e', 'c', 'o', 'n', 'n', 'e',
		'c', 't', '\n', 0x00,
	};

	unsigned char data[32] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteReconnectMessage(writer);

	return writer.tellBit() == sizeof(expected) * 8 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

static bool TestSetViewMessage()
{
	static const unsigned char expected[] =
	{
		0x05, 0x01, 0x02,
	};

	unsigned char data[8] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteSetViewMessage(writer, 513);

	return writer.tellBit() == sizeof(expected) * 8 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

static bool TestSetViewTruncatesToWord()
{
	static const unsigned char expected[] =
	{
		0x05, 0xFF, 0xFF,
	};

	unsigned char data[8] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteSetViewMessage(writer, -1);

	return writer.tellBit() == sizeof(expected) * 8 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

static bool TestSetPauseMessageUsesSingleBitPayload()
{
	unsigned char data[4] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteSetPauseMessage(writer, true);

	return writer.tellBit() == 9 &&
		data[0] == kServiceMessageSetPause &&
		data[1] == 0x01 &&
		!writer.overflow();
}

static bool TestSetPauseFalseClearsPayloadBit()
{
	unsigned char data[4] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteSetPauseMessage(writer, false);

	return writer.tellBit() == 9 &&
		data[0] == kServiceMessageSetPause &&
		data[1] == 0x00 &&
		!writer.overflow();
}

static bool TestVoiceInitMessageAndDefaultCodec()
{
	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteVoiceInitMessage(writer, "", 7);

	NetworkBitBuffer reader(data, writer.tellBit());
	char codec[64] = {};

	return reader.readUnsigned(8) == kServiceMessageVoiceInit &&
		ReadString(reader, codec, sizeof(codec)) &&
		std::strcmp(codec, kServiceMessageDefaultVoiceCodec) == 0 &&
		reader.readUnsigned(8) == 7 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

static bool TestPayloadCanAppendAfterLegacyCommand()
{
	unsigned char data[32] = {};
	data[0] = kServiceMessageStuffText;

	NetworkBitBuffer writer(data, sizeof(data) << 3, 8);
	WriteReconnectPayload(writer);

	return data[0] == kServiceMessageStuffText &&
		writer.tellBit() == 8 + 11 * 8 &&
		std::memcmp(data + 1, kServiceMessageReconnectCommand, 11) == 0 &&
		!writer.overflow();
}

static bool TestOverflow()
{
	unsigned char data[3] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteFileTransferFailedMessage(writer, "too-long");
	return writer.overflow();
}

int main()
{
	if (!TestFileTransferFailedMessage() ||
		!TestReconnectMessage() ||
		!TestSetViewMessage() ||
		!TestSetViewTruncatesToWord() ||
		!TestSetPauseMessageUsesSingleBitPayload() ||
		!TestSetPauseFalseClearsPayloadBit() ||
		!TestVoiceInitMessageAndDefaultCodec() ||
		!TestPayloadCanAppendAfterLegacyCommand() ||
		!TestOverflow())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
