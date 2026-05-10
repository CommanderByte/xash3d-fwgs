#include "utilities/conversion.hpp"

#include <string.h>

namespace xash
{
namespace utilities
{

namespace
{

bool IsEmptyOrNull(const char *string)
{
	return !string || !string[0];
}

int LegacyAtoiCharacter(int sign, const char *str)
{
	return sign * str[1];
}

const char *LegacyAtoiStripWhitespace(const char *str)
{
	while (str && *str == ' ')
		++str;

	return str;
}

}

int LegacyAtoiHex(int sign, const char *str)
{
	int value = 0;

	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
		str += 2;

	while (true)
	{
		const int c = *str++;

		if (c >= '0' && c <= '9')
			value = (value << 4) + c - '0';
		else if (c >= 'a' && c <= 'f')
			value = (value << 4) + c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			value = (value << 4) + c - 'A' + 10;
		else
			return value * sign;
	}
}

int LegacyAtoi(const char *str)
{
	int value = 0;
	int sign;

	if (IsEmptyOrNull(str))
		return 0;

	str = LegacyAtoiStripWhitespace(str);

	if (IsEmptyOrNull(str))
		return 0;

	if (*str == '-')
	{
		sign = -1;
		++str;
	}
	else
	{
		sign = 1;
	}

	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
		return LegacyAtoiHex(sign, str);

	if (str[0] == '\'')
		return LegacyAtoiCharacter(sign, str);

	while (true)
	{
		const int c = *str++;

		if (c < '0' || c > '9')
			return value * sign;

		value = value * 10 + c - '0';
	}
}

float LegacyAtof(const char *str)
{
	double value = 0;
	int sign;
	int decimal;
	int total;

	if (IsEmptyOrNull(str))
		return 0;

	str = LegacyAtoiStripWhitespace(str);

	if (IsEmptyOrNull(str))
		return 0;

	if (*str == '-')
	{
		sign = -1;
		++str;
	}
	else
	{
		sign = 1;
	}

	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
		return static_cast<float>(LegacyAtoiHex(sign, str));

	if (str[0] == '\'')
		return static_cast<float>(LegacyAtoiCharacter(sign, str));

	decimal = -1;
	total = 0;

	while (true)
	{
		const int c = *str++;

		if (c == '.')
		{
			decimal = total;
			continue;
		}

		if (c < '0' || c > '9')
			break;

		value = value * 10 + c - '0';
		++total;
	}

	if (decimal == -1)
		return static_cast<float>(value * sign);

	while (total > decimal)
	{
		value /= 10;
		--total;
	}

	return static_cast<float>(value * sign);
}

void LegacyAtov(float *vec, const char *str, size_t size)
{
	const char *pstr;
	const char *pfront;

	memset(vec, 0, sizeof(*vec) * size);
	pstr = pfront = str;

	for (size_t j = 0; j < size; ++j)
	{
		vec[j] = LegacyAtof(pfront);

		while (*pstr && *pstr != ' ')
			++pstr;

		if (!*pstr)
			break;

		++pstr;
		pfront = pstr;
	}
}

}
}
