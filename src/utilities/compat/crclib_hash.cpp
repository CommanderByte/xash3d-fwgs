#include "utilities/hash.hpp"

extern "C"
{
#include "crclib.h"
}

extern "C" uint COM_HashKey(const char *string, uint hashSize)
{
	return static_cast<uint>(xash::utilities::LegacyHashKey(string, hashSize));
}
