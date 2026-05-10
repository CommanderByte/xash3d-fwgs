#include <stdlib.h>

#include "utilities/conversion.hpp"

using namespace xash::utilities;

static bool NearlyEqual(float left, float right)
{
	const float diff = left > right ? left - right : right - left;
	return diff < 0.0001f;
}

static int TestLegacyAtoi()
{
	if (LegacyAtoi(nullptr) != 0)
		return 1;

	if (LegacyAtoi("") != 0)
		return 2;

	if (LegacyAtoi("   123") != 123)
		return 3;

	if (LegacyAtoi("   ") != 0)
		return 4;

	if (LegacyAtoi("\t123") != 0)
		return 5;

	if (LegacyAtoi("+123") != 0)
		return 6;

	if (LegacyAtoi("-123") != -123)
		return 7;

	if (LegacyAtoi("0xa1ba") != 0xa1ba)
		return 8;

	if (LegacyAtoi("-0XA1BA") != -0xa1ba)
		return 9;

	if (LegacyAtoi("'a'") != 'a')
		return 10;

	if (LegacyAtoi("-'c'") != -'c')
		return 11;

	if (LegacyAtoiHex(1, "a1ba") != 0xa1ba)
		return 12;

	return 0;
}

static int TestLegacyAtof()
{
	if (!NearlyEqual(LegacyAtof(nullptr), 0.0f))
		return 1;

	if (!NearlyEqual(LegacyAtof("   123.123"), 123.123f))
		return 2;

	if (!NearlyEqual(LegacyAtof("-123.13   "), -123.13f))
		return 3;

	if (!NearlyEqual(LegacyAtof("\t123.0"), 0.0f))
		return 4;

	if (!NearlyEqual(LegacyAtof("+123.0"), 0.0f))
		return 5;

	if (!NearlyEqual(LegacyAtof("1.2.3"), 12.3f))
		return 6;

	if (!NearlyEqual(LegacyAtof("-0XA1BA"), -0xa1ba))
		return 7;

	if (!NearlyEqual(LegacyAtof("-'c'"), static_cast<float>(-'c')))
		return 8;

	return 0;
}

static int TestLegacyAtov()
{
	float vec[4];

	LegacyAtov(vec, "1.0 1.2 3", 4);
	if (!NearlyEqual(vec[0], 1.0f) ||
		!NearlyEqual(vec[1], 1.2f) ||
		!NearlyEqual(vec[2], 3.0f) ||
		!NearlyEqual(vec[3], 0.0f))
		return 1;

	LegacyAtov(vec, "1  3", 3);
	if (!NearlyEqual(vec[0], 1.0f) ||
		!NearlyEqual(vec[1], 3.0f) ||
		!NearlyEqual(vec[2], 3.0f))
		return 2;

	LegacyAtov(vec, "9 9 9 9", 0);
	if (!NearlyEqual(vec[0], 1.0f) ||
		!NearlyEqual(vec[1], 3.0f) ||
		!NearlyEqual(vec[2], 3.0f))
		return 3;

	return 0;
}

int main()
{
	int result = TestLegacyAtoi();
	if (result != 0)
		return result;

	result = TestLegacyAtof();
	if (result != 0)
		return result + 32;

	result = TestLegacyAtov();
	if (result != 0)
		return result + 64;

	return EXIT_SUCCESS;
}
