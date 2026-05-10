#include "utilities/text.hpp"

extern "C"
{
#include "crtlib.h"
}

extern "C" char *GAME_EXPORT Q_memfgets(byte *data, int data_len, int *data_offset, char *dst, int dst_size)
{
	return xash::utilities::LegacyMemFgets(data, data_len, data_offset, dst, dst_size);
}

extern "C" void Q_strnlwr(const char *in, char *out, size_t size_out)
{
	xash::utilities::LegacyStrnLower(in, out, size_out);
}
