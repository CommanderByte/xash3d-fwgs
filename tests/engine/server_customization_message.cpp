#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_customization_message.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

static CustomizationMessage Message()
{
	CustomizationMessage message = {};
	message.playerNumber = 3;
	message.type = ResourceType::Decal;
	message.name = "custom.wad";
	message.index = 257;
	message.downloadSize = -42;
	message.flags = kCustomizationMessageFlagCustom | 1U | 2U;
	return message;
}

static bool TestBasicPayloadRoundTrip()
{
	unsigned char data[128] = {};
	std::uint8_t hash[kCustomizationMessageHashSize] = {};

	for (std::size_t i = 0; i < kCustomizationMessageHashSize; ++i)
		hash[i] = static_cast<std::uint8_t>(0xA0 + i);

	CustomizationMessage message = Message();
	message.md5Hash = hash;

	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteCustomizationMessagePayload(writer, message);

	NetworkBitBuffer reader(data, writer.tellBit());
	char name[64] = {};
	std::uint8_t readHash[kCustomizationMessageHashSize] = {};

	if (reader.readUnsigned(8) != 3 ||
		reader.readUnsigned(8) != static_cast<unsigned int>(ResourceType::Decal))
	{
		return false;
	}

	for (std::size_t i = 0; i < sizeof(name); ++i)
	{
		name[i] = static_cast<char>(reader.readUnsigned(8));
		if (name[i] == '\0')
			break;
	}

	if (std::strcmp(name, "custom.wad") != 0 ||
		reader.readSigned(16) != 257 ||
		reader.readSigned(32) != -42 ||
		reader.readUnsigned(8) != message.flags)
	{
		return false;
	}

	if (!reader.readBits(readHash, kCustomizationMessageHashSize << 3))
		return false;

	return std::memcmp(readHash, hash, sizeof(hash)) == 0 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

static bool TestGoldenBytesForCustomPayload()
{
	static const unsigned char kExpected[] =
	{
		0x03, 0x03, 0x63, 0x75, 0x73, 0x74, 0x6F, 0x6D,
		0x2E, 0x77, 0x61, 0x64, 0x00, 0x01, 0x01, 0xD6,
		0xFF, 0xFF, 0xFF, 0x07, 0xA0, 0xA1, 0xA2, 0xA3,
		0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB,
		0xAC, 0xAD, 0xAE, 0xAF,
	};

	unsigned char data[64] = {};
	std::uint8_t hash[kCustomizationMessageHashSize] = {};

	for (std::size_t i = 0; i < kCustomizationMessageHashSize; ++i)
		hash[i] = static_cast<std::uint8_t>(0xA0 + i);

	CustomizationMessage message = Message();
	message.md5Hash = hash;

	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteCustomizationMessagePayload(writer, message);

	return writer.tellBit() == sizeof(kExpected) * 8 &&
		std::memcmp(data, kExpected, sizeof(kExpected)) == 0 &&
		!writer.overflow();
}

static bool TestNonCustomPayloadOmitsHash()
{
	unsigned char data[64] = {};
	CustomizationMessage message = Message();
	message.flags = 1U;

	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteCustomizationMessagePayload(writer, message);

	NetworkBitBuffer reader(data, writer.tellBit());

	reader.readUnsigned(8);
	reader.readUnsigned(8);

	for (;;)
	{
		if (reader.readUnsigned(8) == 0)
			break;
	}

	reader.readSigned(16);
	reader.readSigned(32);

	return reader.readUnsigned(8) == 1U &&
		reader.tellBit() == writer.tellBit() &&
		writer.tellBit() == 20 * 8 &&
		!writer.overflow();
}

static bool TestOverflow()
{
	unsigned char data[8] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteCustomizationMessagePayload(writer, Message());
	return writer.overflow();
}

int main()
{
	if (!TestBasicPayloadRoundTrip() ||
		!TestGoldenBytesForCustomPayload() ||
		!TestNonCustomPayloadOmitsHash() ||
		!TestOverflow())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
