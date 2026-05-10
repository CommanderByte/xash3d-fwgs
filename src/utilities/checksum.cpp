#include "utilities/checksum.hpp"

#include <string.h>

namespace xash
{
namespace utilities
{

namespace
{

uint32_t GenerateCrc32TableEntry(uint32_t index)
{
	uint32_t value = index;

	for (int bit = 0; bit < 8; ++bit)
	{
		if (value & 1u)
			value = (value >> 1) ^ 0xEDB88320u;
		else
			value >>= 1;
	}

	return value;
}

struct Crc32TableStorage
{
	uint32_t values[kCrc32TableSize];

	Crc32TableStorage()
	{
		for (uint32_t index = 0; index < kCrc32TableSize; ++index)
			values[index] = GenerateCrc32TableEntry(index);
	}
};

void WriteLittleEndian32(uint8_t *dst, uint32_t value)
{
	dst[0] = static_cast<uint8_t>(value & 0xFFu);
	dst[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
	dst[2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
	dst[3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
}

}

const uint32_t *Crc32Table()
{
	static const Crc32TableStorage table;

	return table.values;
}

uint32_t Crc32TableEntry(uint32_t index)
{
	if (index < kCrc32TableSize)
		return Crc32Table()[index];

	return GenerateCrc32TableEntry(index);
}

uint32_t Crc32ProcessByte(uint32_t crc, uint8_t value)
{
	return Crc32Table()[(crc ^ value) & 0xFFu] ^ (crc >> 8);
}

uint32_t Crc32ProcessBuffer(uint32_t crc, const void *buffer, int length)
{
	const uint8_t *bytes = static_cast<const uint8_t *>(buffer);
	const uint32_t *table = Crc32Table();

	while (length-- > 0)
		crc = table[(crc ^ *bytes++) & 0xFFu] ^ (crc >> 8);

	return crc;
}

uint32_t Crc32Final(uint32_t crc)
{
	return crc ^ kCrc32XorValue;
}

uint8_t Crc32BlockSequence(const uint8_t *base, int length, int sequence)
{
	uint8_t buffer[64];
	uint8_t tableBytes[8];
	int offset;

	if (sequence < 0)
		sequence = -sequence;

	if (length > 60)
		length = 60;

	memcpy(buffer, base, length);

	offset = sequence % 0x3FC;
	WriteLittleEndian32(tableBytes, Crc32TableEntry(static_cast<uint32_t>(offset / 4)));
	WriteLittleEndian32(tableBytes + 4, Crc32TableEntry(static_cast<uint32_t>(offset / 4 + 1)));
	memcpy(buffer + length, tableBytes + (offset & 3), 4);
	length += 4;

	uint32_t crc = Crc32ProcessBuffer(kCrc32InitialValue, buffer, length);
	return static_cast<uint8_t>(Crc32Final(crc));
}

}
}
