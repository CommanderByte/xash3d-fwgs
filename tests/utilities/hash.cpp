#include <stdint.h>
#include <stdlib.h>

#include "utilities/checksum.hpp"
#include "utilities/hash.hpp"

using namespace xash::utilities;

static int TestLegacyHashKey()
{
	if (LegacyHashKey("", 64) != 5u)
		return 1;

	if (LegacyHashKey("ABC", 64) != 43u)
		return 2;

	if (LegacyHashKey("ABC", 4096) != LegacyHashKey("abc", 4096))
		return 3;

	if (LegacyHashKey("AbC/Path-01", 31) != 20u)
		return 4;

	if (LegacyHashKey("sound/weapons/pl_gun3.wav", 4096) != 989u)
		return 5;

	if (LegacyHashKey("textures/{BLUE", 4096) != 347u)
		return 6;

	if (LegacyHashKey("ABC", 0) != 193450027u)
		return 7;

	return 0;
}

static int TestCrc32()
{
	static const unsigned char digits[] = "123456789";
	static const unsigned char name[] = "xash3d-fwgs";
	unsigned char incremental[] = "123456789";
	uint32_t crc = kCrc32InitialValue;

	crc = Crc32ProcessBuffer(crc, digits, 9);
	if (crc != 0x340BC6D9u)
		return 1;

	if (Crc32Final(crc) != 0xCBF43926u)
		return 2;

	crc = kCrc32InitialValue;
	for (int i = 0; i < 9; ++i)
		crc = Crc32ProcessByte(crc, incremental[i]);

	if (Crc32Final(crc) != 0xCBF43926u)
		return 3;

	crc = Crc32ProcessBuffer(kCrc32InitialValue, name, 11);
	if (crc != 0xBB60C42Fu || Crc32Final(crc) != 0x449F3BD0u)
		return 4;

	if (Crc32Final(Crc32ProcessBuffer(kCrc32InitialValue, "", 0)) != 0u)
		return 5;

	return 0;
}

static int TestCrc32BlockSequence()
{
	static const unsigned char empty[] = "";
	static const unsigned char abc[] = "abc";
	static const unsigned char longData[] =
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	if (Crc32BlockSequence(empty, 0, 0) != 28u)
		return 1;

	if (Crc32BlockSequence(abc, 3, 17) != 183u)
		return 2;

	if (Crc32BlockSequence(abc, 3, -7) != 163u)
		return 3;

	if (Crc32BlockSequence(longData, 62, 0) != 103u)
		return 4;

	if (Crc32BlockSequence(abc, 3, 1020) != Crc32BlockSequence(abc, 3, 0))
		return 5;

	return 0;
}

int main()
{
	int result = TestLegacyHashKey();
	if (result != 0)
		return result;

	result = TestCrc32();
	if (result != 0)
		return result + 16;

	result = TestCrc32BlockSequence();
	if (result != 0)
		return result + 32;

	return EXIT_SUCCESS;
}
