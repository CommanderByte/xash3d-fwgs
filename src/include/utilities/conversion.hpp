#ifndef XASH_UTILITIES_CONVERSION_HPP
#define XASH_UTILITIES_CONVERSION_HPP

#include <stddef.h>

namespace xash
{
namespace utilities
{

int LegacyAtoiHex(int sign, const char *str);
int LegacyAtoi(const char *str);
float LegacyAtof(const char *str);
void LegacyAtov(float *vec, const char *str, size_t size);

}
}

#endif
