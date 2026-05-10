#ifndef XASH_UTILITIES_TEXT_HPP
#define XASH_UTILITIES_TEXT_HPP

#include <stddef.h>
#include <stdint.h>

namespace xash
{
namespace utilities
{

void LegacyStrnLower(const char *in, char *out, size_t sizeOut);
char *LegacyMemFgets(uint8_t *data, int dataLen, int *dataOffset, char *dst, int dstSize);

}
}

#endif
