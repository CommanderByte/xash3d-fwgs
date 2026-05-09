#ifndef XASH_UTILITIES_HASH_HPP
#define XASH_UTILITIES_HASH_HPP

#include <stdint.h>

namespace xash
{
namespace utilities
{

uint32_t LegacyCaseInsensitiveHash(const char *string);
uint32_t LegacyHashKey(const char *string, uint32_t hashSize);

}
}

#endif
