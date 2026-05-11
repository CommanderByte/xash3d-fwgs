#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/messaging/server_resource_message.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

static ResourceMessageRow Row()
{
	ResourceMessageRow row = {};
	row.type = ResourceType::Model;
	row.name = "models/w_test.mdl";
	row.index = 37;
	row.downloadSize = 123456;
	row.flags = kResourceMessageFlagFatalIfMissing;
	return row;
}

static bool TestBasicRowRoundTrip()
{
	unsigned char data[128] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	ResourceMessageRow row = Row();

	WriteResourceMessageRow(writer, row);

	NetworkBitBuffer reader(data, writer.tellBit());
	char name[64] = {};
	char ch = '\0';
	std::size_t nameLength = 0;

	if (reader.readUnsigned(4) != static_cast<unsigned int>(ResourceType::Model))
		return false;

	do
	{
		ch = static_cast<char>(reader.readUnsigned(8));
		if (nameLength < sizeof(name))
			name[nameLength++] = ch;
	} while (ch != '\0' && nameLength < sizeof(name));

	return std::strcmp(name, "models/w_test.mdl") == 0 &&
		reader.readUnsigned(kResourceMessageModelIndexBits) == 37 &&
		reader.readSigned(kResourceMessageDownloadSizeBits) == 123456 &&
		reader.readUnsigned(kResourceMessageFlagsBits) == kResourceMessageFlagFatalIfMissing &&
		reader.readOneBit() == 0 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

static bool TestCustomHashAndReservedPayload()
{
	unsigned char data[256] = {};
	std::uint8_t hash[kResourceMessageHashSize] = {};
	std::uint8_t reserved[kResourceMessageReservedSize] = {};

	for (std::size_t i = 0; i < kResourceMessageHashSize; ++i)
		hash[i] = static_cast<std::uint8_t>(0xA0 + i);

	for (std::size_t i = 0; i < kResourceMessageReservedSize; ++i)
		reserved[i] = static_cast<std::uint8_t>(i + 1);

	ResourceMessageRow row = {};
	row.type = ResourceType::Decal;
	row.name = "custom.hpk";
	row.index = 5;
	row.downloadSize = -42;
	row.flags = kResourceMessageFlagFatalIfMissing |
		kResourceMessageFlagWasMissing |
		kResourceMessageFlagCustom;
	row.md5Hash = hash;
	row.reservedData = reserved;

	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteResourceMessageRow(writer, row);

	NetworkBitBuffer reader(data, writer.tellBit());
	char name[32] = {};
	std::uint8_t readHash[kResourceMessageHashSize] = {};
	std::uint8_t readReserved[kResourceMessageReservedSize] = {};

	if (reader.readUnsigned(4) != static_cast<unsigned int>(ResourceType::Decal))
		return false;

	for (std::size_t i = 0; i < sizeof(name); ++i)
	{
		name[i] = static_cast<char>(reader.readUnsigned(8));
		if (name[i] == '\0')
			break;
	}

	if (std::strcmp(name, "custom.hpk") != 0 ||
		reader.readUnsigned(kResourceMessageModelIndexBits) != 5 ||
		reader.readSigned(kResourceMessageDownloadSizeBits) != -42 ||
		reader.readUnsigned(kResourceMessageFlagsBits) !=
			(kResourceMessageFlagFatalIfMissing | kResourceMessageFlagWasMissing))
	{
		return false;
	}

	if (!reader.readBits(readHash, kResourceMessageHashSize << 3))
		return false;

	if (reader.readOneBit() != 1)
		return false;

	if (!reader.readBits(readReserved, kResourceMessageReservedSize << 3))
		return false;

	return std::memcmp(readHash, hash, sizeof(hash)) == 0 &&
		std::memcmp(readReserved, reserved, sizeof(reserved)) == 0 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

static bool TestGoldenBytesForSimpleRows()
{
	static const unsigned char kExpected[] =
	{
		0xD2, 0xF6, 0x46, 0x56, 0xC6, 0x36, 0xF7, 0x72,
		0xF7, 0x45, 0x57, 0x36, 0x47, 0xE7, 0xD2, 0x46,
		0xC6, 0x06, 0x50, 0x02, 0x40, 0xE2, 0x01, 0x01,
	};

	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteResourceMessageRow(writer, Row());

	return writer.tellBit() == 188 &&
		std::memcmp(data, kExpected, sizeof(kExpected)) == 0 &&
		!writer.overflow();
}

static bool TestReservedDataPresence()
{
	std::uint8_t empty[kResourceMessageReservedSize] = {};
	std::uint8_t reserved[kResourceMessageReservedSize] = {};
	ResourceMessageRow row = Row();

	row.reservedData = nullptr;
	if (ResourceMessageRowHasReservedData(row))
		return false;

	row.reservedData = empty;
	if (ResourceMessageRowHasReservedData(row))
		return false;

	reserved[31] = 1;
	row.reservedData = reserved;
	return ResourceMessageRowHasReservedData(row);
}

static bool TestOverflow()
{
	unsigned char data[2] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteResourceMessageRow(writer, Row());
	return writer.overflow();
}

int main()
{
	if (!TestBasicRowRoundTrip() ||
		!TestCustomHashAndReservedPayload() ||
		!TestGoldenBytesForSimpleRows() ||
		!TestReservedDataPresence() ||
		!TestOverflow())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
