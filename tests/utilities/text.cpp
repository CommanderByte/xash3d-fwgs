#include <stdlib.h>
#include <string.h>

#include "utilities/text.hpp"

using namespace xash::utilities;

static int TestLegacyStrnLower()
{
	char text[16];
	char tiny[4];

	LegacyStrnLower("ASDFGKJ", text, sizeof(text));
	if (strcmp(text, "asdfgkj") != 0)
		return 1;

	memset(tiny, 'x', sizeof(tiny));
	LegacyStrnLower("ABCD", tiny, sizeof(tiny));
	if (strcmp(tiny, "abc") != 0)
		return 2;

	LegacyStrnLower("ABC", tiny, 0);
	if (strcmp(tiny, "abc") != 0)
		return 3;

	return 0;
}

static int TestLegacyMemFgets()
{
	uint8_t data[] = {
		'F', 'i', 'r', 's', 't', '\n',
		'S', 'e', 'c', 'o', 'n', 'd'
	};
	char buffer[16];
	int offset = 0;

	if (LegacyMemFgets(data, sizeof(data), &offset, buffer, sizeof(buffer)) != buffer)
		return 1;

	if (strcmp(buffer, "First\n") != 0 || offset != 6)
		return 2;

	if (LegacyMemFgets(data, sizeof(data), &offset, buffer, 4) != buffer)
		return 3;

	if (strcmp(buffer, "Sec") != 0 || offset != 12)
		return 4;

	if (LegacyMemFgets(data, sizeof(data), &offset, buffer, sizeof(buffer)) != nullptr)
		return 5;

	return 0;
}

int main()
{
	int result = TestLegacyStrnLower();
	if (result != 0)
		return result;

	result = TestLegacyMemFgets();
	if (result != 0)
		return result + 16;

	return EXIT_SUCCESS;
}
