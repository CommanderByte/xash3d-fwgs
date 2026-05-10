#include "utilities/text.hpp"

#include <string.h>

namespace xash
{
namespace utilities
{

namespace
{

char ToLowerAscii(char value)
{
	if (value >= 'A' && value <= 'Z')
		return static_cast<char>(value + 'a' - 'A');

	return value;
}

size_t BoundedStringCopy(char *dst, const char *src, size_t size)
{
	if (!dst || !src || size == 0)
		return 0;

	const size_t len = strlen(src);
	const size_t copy = len >= size ? size - 1 : len;

	memcpy(dst, src, copy);
	dst[copy] = '\0';

	return len;
}

void BoundedMemoryLineCopy(char *dst, const char *src, int count, int dstSize)
{
	if (dstSize <= 0)
		return;

	const int copy = count < dstSize - 1 ? count : dstSize - 1;
	if (copy > 0)
		memcpy(dst, src, static_cast<size_t>(copy));

	dst[copy] = '\0';
}

}

void LegacyStrnLower(const char *in, char *out, size_t sizeOut)
{
	const size_t len = BoundedStringCopy(out, in, sizeOut);
	const size_t writable = sizeOut > 0 ? sizeOut - 1 : 0;
	const size_t lower = len < writable ? len : writable;

	for (size_t i = 0; i < lower; ++i)
		out[i] = ToLowerAscii(out[i]);
}

char *LegacyMemFgets(uint8_t *data, int dataLen, int *dataOffset, char *dst, int dstSize)
{
	if (!data || !dataOffset || !dst || *dataOffset >= dataLen)
		return nullptr;

	const char *start = reinterpret_cast<const char *>(data) + *dataOffset;
	int remaining = dataLen - *dataOffset;
	const char *end = static_cast<const char *>(memchr(start, '\n', static_cast<size_t>(remaining)));

	if (end)
		remaining = static_cast<int>(end - start) + 1;

	BoundedMemoryLineCopy(dst, start, remaining, dstSize);
	*dataOffset += remaining;

	return dst;
}

}
}
