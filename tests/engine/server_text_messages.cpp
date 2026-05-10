#include <cstdlib>
#include <cstddef>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_text_messages.hpp"

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

static bool TestPrintMessage()
{
	static const unsigned char expected[] =
	{
		0x08, 'h', 'e', 'l', 'l', 'o', '\n', 0x00,
	};

	unsigned char data[32] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WritePrintMessage(writer, "hello\n");

	return writer.tellBit() == sizeof(expected) * 8 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

static bool TestStuffTextMessage()
{
	static const unsigned char expected[] =
	{
		0x09, 'f', 'u', 'l', 'l', 's', 'e', 'r',
		'v', 'e', 'r', 'i', 'n', 'f', 'o', ' ',
		'"', '\\', 'g', 'a', 'm', 'e', '\\', 'v',
		'a', 'l', 'v', 'e', '"', '\n', 0x00,
	};

	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteStuffTextMessage(writer, "fullserverinfo \"\\game\\valve\"\n");

	return writer.tellBit() == sizeof(expected) * 8 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

static bool TestPayloadCanAppendAfterLegacyPrintCommand()
{
	unsigned char data[32] = {};
	data[0] = kTextMessagePrint;

	NetworkBitBuffer writer(data, sizeof(data) << 3, 8);
	WritePrintPayload(writer, "status");

	return data[0] == kTextMessagePrint &&
		writer.tellBit() == 8 + 7 * 8 &&
		std::memcmp(data + 1, "status", 7) == 0 &&
		!writer.overflow();
}

static bool TestPayloadCanAppendAfterLegacyStuffTextCommand()
{
	unsigned char data[32] = {};
	data[0] = kTextMessageStuffText;

	NetworkBitBuffer writer(data, sizeof(data) << 3, 8);
	WriteStuffTextPayload(writer, "reconnect\n");

	return data[0] == kTextMessageStuffText &&
		writer.tellBit() == 8 + 11 * 8 &&
		std::memcmp(data + 1, "reconnect\n", 11) == 0 &&
		!writer.overflow();
}

static bool TestNullTextSerializesAsEmptyString()
{
	static const unsigned char expected[] =
	{
		0x08, 0x00,
	};

	unsigned char data[8] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WritePrintMessage(writer, nullptr);

	return writer.tellBit() == sizeof(expected) * 8 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

static bool TestPayloadRoundTrip()
{
	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteStuffTextMessage(writer, "cmd arg\n");

	NetworkBitBuffer reader(data, writer.tellBit());
	char text[64] = {};

	return reader.readUnsigned(8) == kTextMessageStuffText &&
		ReadString(reader, text, sizeof(text)) &&
		std::strcmp(text, "cmd arg\n") == 0 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

static bool TestOverflow()
{
	unsigned char data[4] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WritePrintMessage(writer, "too-long");
	return writer.overflow();
}

int main()
{
	if (!TestPrintMessage() ||
		!TestStuffTextMessage() ||
		!TestPayloadCanAppendAfterLegacyPrintCommand() ||
		!TestPayloadCanAppendAfterLegacyStuffTextCommand() ||
		!TestNullTextSerializesAsEmptyString() ||
		!TestPayloadRoundTrip() ||
		!TestOverflow())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
