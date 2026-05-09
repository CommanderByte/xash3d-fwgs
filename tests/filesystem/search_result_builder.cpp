#include <stdlib.h>
#include <string.h>

#include "filesystem/search_result_builder.hpp"

using namespace xash::filesystem;

static bool TestSortAndCompact()
{
	char alpha[] = "alpha.txt";
	char sameA[] = "same.txt";
	char sameB[] = "same.txt";
	char zeta[] = "zeta.txt";
	char *strings[] = { sameA, zeta, alpha, sameB };
	const size_t count = sizeof(strings) / sizeof(strings[0]);
	const size_t compacted = 3;

	SearchResultBuilder::sort(strings, count);

	if (strcmp(strings[0], "alpha.txt") ||
		strcmp(strings[1], "same.txt") ||
		strcmp(strings[2], "same.txt") ||
		strcmp(strings[3], "zeta.txt"))
	{
		return false;
	}

	return SearchResultBuilder::compactDuplicates(strings, count) == compacted &&
		strcmp(strings[0], "alpha.txt") == 0 &&
		strcmp(strings[1], "same.txt") == 0 &&
		strcmp(strings[2], "zeta.txt") == 0;
}

static bool TestPackedCopy()
{
	char first[] = "alpha.txt";
	char second[] = "zeta.txt";
	char *strings[] = { first, second };
	char *filenames[2] = {};
	char buffer[32] = {};
	const size_t bytes = SearchResultBuilder::packedStringBytes(strings, 2);

	if (bytes != strlen(first) + 1 + strlen(second) + 1)
		return false;

	SearchResultBuilder::copyPacked(strings, 2, filenames, buffer);

	return filenames[0] == buffer &&
		filenames[1] == buffer + strlen(first) + 1 &&
		strcmp(filenames[0], first) == 0 &&
		strcmp(filenames[1], second) == 0;
}

static bool TestEmptyAndNullStrings()
{
	char value[] = "value";
	char *strings[] = { value, nullptr };
	char *filenames[2] = {};
	char buffer[16] = {};

	SearchResultBuilder::sort(strings, 2);

	if (strings[0] != nullptr || strcmp(strings[1], "value"))
		return false;

	if (SearchResultBuilder::packedStringBytes(strings, 2) != strlen(value) + 2)
		return false;

	SearchResultBuilder::copyPacked(strings, 2, filenames, buffer);

	return strcmp(filenames[0], "") == 0 &&
		strcmp(filenames[1], "value") == 0;
}

int main()
{
	if (!TestSortAndCompact() ||
		!TestPackedCopy() ||
		!TestEmptyAndNullStrings())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
