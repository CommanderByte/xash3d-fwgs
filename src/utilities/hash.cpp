#include "utilities/hash.hpp"

namespace xash
{
namespace utilities
{

namespace
{

unsigned char ToLowerAscii(unsigned char value)
{
	if (value >= 'A' && value <= 'Z')
		return static_cast<unsigned char>(value + 'a' - 'A');

	return value;
}

}

uint32_t LegacyCaseInsensitiveHash(const char *string)
{
	uint32_t hashKey = 5381;
	unsigned char value;

	while ((value = static_cast<unsigned char>(*string++)) != 0)
	{
		value = ToLowerAscii(value);
		hashKey = (hashKey << 5) + hashKey + (value & 0xDFu);
	}

	return hashKey;
}

uint32_t LegacyHashKey(const char *string, uint32_t hashSize)
{
	return LegacyCaseInsensitiveHash(string) & (hashSize - 1u);
}

}
}
