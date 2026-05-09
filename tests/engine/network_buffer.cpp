#include <stdlib.h>
#include <string.h>

#include "engine/network/network_buffer.hpp"

using namespace xash::engine::network;

static const unsigned char kGoldenBuffer[] =
{
	'a', 's', 'd', 'f', 0xba, 0xa1, 0xba, 0xa1, 0xed, 0xc8, 0x15, 0x7a,
};

static const size_t kGoldenBits = ((4 + 4 + 2 + 1) << 3) + 4;

static bool TestBitsToBytes()
{
	return NetworkBufferBitsToBytes(0) == 0 &&
		NetworkBufferBitsToBytes(1) == 1 &&
		NetworkBufferBitsToBytes(8) == 1 &&
		NetworkBufferBitsToBytes(9) == 2;
}

static bool TestGoldenWrite()
{
	unsigned char data[0x100] = {};
	NetworkBitBuffer buffer(data, sizeof(data) << 3);

	buffer.writeBits("asdf", 4 << 3);
	buffer.writeUnsigned(0xa1baa1baU, 32);
	buffer.writeSigned(-0x3713, 16);
	buffer.writeOneBit(1);
	buffer.writeOneBit(0);
	buffer.writeOneBit(1);
	buffer.writeOneBit(0);
	buffer.writeUnsigned(0xa1, 8);

	if (buffer.overflow() || buffer.tellBit() != kGoldenBits)
		return false;

	if (memcmp(data, kGoldenBuffer, kGoldenBits >> 3) != 0)
		return false;

	buffer.seekToBit(kGoldenBits & ~7);
	return buffer.readUnsigned(4) == 0xa;
}

static bool TestGoldenRead()
{
	unsigned char text[4] = {};
	NetworkBitBuffer buffer(const_cast<unsigned char *>(kGoldenBuffer), kGoldenBits);

	if (!buffer.readBits(text, 4 << 3))
		return false;

	if (memcmp(text, "asdf", 4) != 0)
		return false;

	return buffer.readUnsigned(16) == 0xa1ba &&
		buffer.readUnsigned(32) == 0xc8eda1baU &&
		buffer.readOneBit() == 1 &&
		buffer.readOneBit() == 0 &&
		buffer.readOneBit() == 1 &&
		buffer.readOneBit() == 0 &&
		buffer.readUnsigned(8) == 0xa1 &&
		buffer.tellBit() == kGoldenBits &&
		!buffer.overflow();
}

static bool TestSignedRoundTrip()
{
	unsigned char data[8] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);

	writer.writeSigned(-5, 8);
	writer.startAlternateSign();
	writer.writeSigned(-6, 8);
	if (!writer.endAlternateSign())
		return false;

	NetworkBitBuffer reader(data, writer.tellBit());
	const int first = reader.readSigned(8);
	reader.startAlternateSign();
	const int second = reader.readSigned(8);

	return first == -5 &&
		second == -6 &&
		!reader.overflow();
}

static bool TestOverflowQuirks()
{
	unsigned char data[1] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);

	writer.writeUnsigned(0x1ff, 9);
	if (!writer.overflow() || writer.tellBit() != writer.maxBits())
		return false;

	NetworkBitBuffer shortReader(data, 4);
	if (shortReader.readUnsigned(8) != 0)
		return false;

	if (shortReader.overflow() || shortReader.tellBit() != 0)
		return false;

	return shortReader.readUnsigned(5) == 0 &&
		shortReader.overflow() &&
		shortReader.tellBit() == 4;
}

static bool TestExciseBits()
{
	unsigned char first[0x100] = {};
	unsigned char second[0x100] = {};

	memcpy(first, kGoldenBuffer, NetworkBufferBitsToBytes(kGoldenBits));
	NetworkBitBuffer firstBuffer(first, kGoldenBits);

	if (!firstBuffer.exciseBits(8, 28))
		return false;

	const unsigned char firstExpected[] = { 'a', 0x1b, 0xaa, 0x1b, 0xda, 0x8e, 0x5c, 0xa1 };

	if (firstBuffer.maxBits() != 64 || memcmp(first, firstExpected, sizeof(firstExpected)) != 0)
		return false;

	memcpy(second, kGoldenBuffer, NetworkBufferBitsToBytes(kGoldenBits));
	NetworkBitBuffer secondBuffer(second, kGoldenBits);

	if (!secondBuffer.exciseBits(16, 32))
		return false;

	const unsigned char secondExpected[] = { 'a', 's', 0xba, 0xa1, 0xed, 0xc8, 0x15 };

	if (secondBuffer.maxBits() != kGoldenBits - 32 ||
		memcmp(second, secondExpected, sizeof(secondExpected)) != 0)
	{
		return false;
	}

	secondBuffer.seekToBit(7 << 3);
	return secondBuffer.readUnsigned(4) == 0xa;
}

int main()
{
	if (!TestBitsToBytes() ||
		!TestGoldenWrite() ||
		!TestGoldenRead() ||
		!TestSignedRoundTrip() ||
		!TestOverflowQuirks() ||
		!TestExciseBits())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
