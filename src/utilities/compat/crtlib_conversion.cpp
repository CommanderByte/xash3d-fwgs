#include "utilities/conversion.hpp"

extern "C"
{
#include "crtlib.h"
}

extern "C" int Q_atoi_hex(int sign, const char *str)
{
	return xash::utilities::LegacyAtoiHex(sign, str);
}

extern "C" int Q_atoi(const char *str)
{
	return xash::utilities::LegacyAtoi(str);
}

extern "C" float Q_atof(const char *str)
{
	return xash::utilities::LegacyAtof(str);
}

extern "C" void Q_atov(float *vec, const char *str, size_t size)
{
	xash::utilities::LegacyAtov(vec, str, size);
}
