#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_userinfo_message.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

static UserinfoUpdatePayload Payload()
{
	static const std::uint8_t digest[kUserinfoDigestSize] =
	{
		0x00, 0x11, 0x22, 0x33,
		0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xAA, 0xBB,
		0xCC, 0xDD, 0xEE, 0xFF,
	};

	UserinfoUpdatePayload payload = {};
	payload.clientIndex = 17;
	payload.userId = -1234567;
	payload.active = true;
	payload.userinfo = "\\name\\gordon\\model\\gordon";
	payload.hashedCdKeyDigest = digest;
	return payload;
}

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

static bool TestActivePayloadRoundTrip()
{
	unsigned char data[128] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	UserinfoUpdatePayload payload = Payload();

	WriteUserinfoUpdatePayload(writer, payload);

	NetworkBitBuffer reader(data, writer.tellBit());
	char info[64] = {};
	std::uint8_t digest[kUserinfoDigestSize] = {};

	if (reader.readUnsigned(kUserinfoClientIndexBits) != 17 ||
		reader.readSigned(32) != -1234567 ||
		reader.readOneBit() != 1 ||
		!ReadString(reader, info, sizeof(info)) ||
		std::strcmp(info, "\\name\\gordon\\model\\gordon") != 0 ||
		!reader.readBits(digest, kUserinfoDigestSize << 3))
	{
		return false;
	}

	return std::memcmp(digest, payload.hashedCdKeyDigest, sizeof(digest)) == 0 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

static bool TestInactivePayloadOmitsInfoAndDigest()
{
	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	UserinfoUpdatePayload payload = Payload();
	payload.active = false;
	payload.userinfo = "\\name\\should_not_emit";

	WriteUserinfoUpdatePayload(writer, payload);

	NetworkBitBuffer reader(data, writer.tellBit());
	return reader.readUnsigned(kUserinfoClientIndexBits) == 17 &&
		reader.readSigned(32) == -1234567 &&
		reader.readOneBit() == 0 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

static bool TestNullDigestEmitsZeros()
{
	unsigned char data[128] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	UserinfoUpdatePayload payload = Payload();
	payload.userinfo = "";
	payload.hashedCdKeyDigest = nullptr;

	WriteUserinfoUpdatePayload(writer, payload);

	NetworkBitBuffer reader(data, writer.tellBit());
	char info[4] = {};
	std::uint8_t digest[kUserinfoDigestSize] = {};

	if (reader.readUnsigned(kUserinfoClientIndexBits) != 17 ||
		reader.readSigned(32) != -1234567 ||
		reader.readOneBit() != 1 ||
		!ReadString(reader, info, sizeof(info)) ||
		std::strcmp(info, "") != 0 ||
		!reader.readBits(digest, kUserinfoDigestSize << 3))
	{
		return false;
	}

	static const std::uint8_t zeros[kUserinfoDigestSize] = {};
	return std::memcmp(digest, zeros, sizeof(digest)) == 0 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

static bool TestGoldenActivePayloadBytes()
{
	static const unsigned char expected[] =
	{
		0x31, 0x2F, 0xA5, 0xFD, 0x3F, 0x97, 0x5B, 0x58,
		0x5B, 0x19, 0xD7, 0xD9, 0x9B, 0x1C, 0xD9, 0x9B,
		0x1B, 0x57, 0xDB, 0x1B, 0x59, 0x19, 0x1B, 0xD7,
		0xD9, 0x9B, 0x1C, 0xD9, 0x9B, 0x1B, 0x00, 0x40,
		0x84, 0xC8, 0x0C, 0x51, 0x95, 0xD9, 0x1D, 0x62,
		0xA6, 0xEA, 0x2E, 0x73, 0xB7, 0xFB, 0x3F,
	};

	unsigned char data[128] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteUserinfoUpdatePayload(writer, Payload());

	return writer.tellBit() == 374 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

static bool TestPayloadCanAppendAfterCommandByte()
{
	unsigned char data[128] = {};
	data[0] = 13;

	NetworkBitBuffer writer(data, sizeof(data) << 3, 8);
	UserinfoUpdatePayload payload = Payload();

	WriteUserinfoUpdatePayload(writer, payload);

	NetworkBitBuffer reader(data, writer.tellBit(), 8);
	char info[64] = {};
	std::uint8_t digest[kUserinfoDigestSize] = {};

	if (data[0] != 13 ||
		reader.readUnsigned(kUserinfoClientIndexBits) != 17 ||
		reader.readSigned(32) != -1234567 ||
		reader.readOneBit() != 1 ||
		!ReadString(reader, info, sizeof(info)) ||
		std::strcmp(info, "\\name\\gordon\\model\\gordon") != 0 ||
		!reader.readBits(digest, kUserinfoDigestSize << 3))
	{
		return false;
	}

	return std::memcmp(digest, payload.hashedCdKeyDigest, sizeof(digest)) == 0 &&
		writer.tellBit() == 382 &&
		reader.tellBit() == writer.tellBit() &&
		!writer.overflow() &&
		!reader.overflow();
}

static bool TestOverflow()
{
	unsigned char data[4] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteUserinfoUpdatePayload(writer, Payload());
	return writer.overflow();
}

int main()
{
	if (!TestActivePayloadRoundTrip() ||
		!TestInactivePayloadOmitsInfoAndDigest() ||
		!TestNullDigestEmitsZeros() ||
		!TestGoldenActivePayloadBytes() ||
		!TestPayloadCanAppendAfterCommandByte() ||
		!TestOverflow())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
