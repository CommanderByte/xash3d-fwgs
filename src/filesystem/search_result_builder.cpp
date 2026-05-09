#include "filesystem/search_result_builder.hpp"

#include <string.h>

namespace xash
{
namespace filesystem
{

void SearchResultBuilder::sort(char **strings, size_t count)
{
	for (size_t i = 0; i + 1 < count; ++i)
	{
		for (size_t j = i + 1; j < count; ++j)
		{
			if (compare(strings[i], strings[j]) > 0)
			{
				char *tmp = strings[i];
				strings[i] = strings[j];
				strings[j] = tmp;
			}
		}
	}
}

size_t SearchResultBuilder::compactDuplicates(char **strings, size_t count)
{
	if (count == 0)
		return 0;

	size_t write = 1;

	for (size_t read = 1; read < count; ++read)
	{
		if (compare(strings[write - 1], strings[read]) == 0)
			continue;

		strings[write++] = strings[read];
	}

	return write;
}

size_t SearchResultBuilder::packedStringBytes(char *const *strings, size_t count)
{
	size_t bytes = 0;

	for (size_t i = 0; i < count; ++i)
		bytes += length(strings[i]) + 1;

	return bytes;
}

void SearchResultBuilder::copyPacked(
	char *const *strings,
	size_t count,
	char **filenames,
	char *buffer)
{
	size_t offset = 0;

	for (size_t i = 0; i < count; ++i)
	{
		const size_t bytes = length(strings[i]) + 1;
		filenames[i] = buffer + offset;
		if (strings[i] == nullptr)
			filenames[i][0] = '\0';
		else
			memcpy(filenames[i], strings[i], bytes);
		offset += bytes;
	}
}

int SearchResultBuilder::compare(const char *a, const char *b)
{
	if (a == nullptr)
		a = "";
	if (b == nullptr)
		b = "";

	return strcmp(a, b);
}

size_t SearchResultBuilder::length(const char *value)
{
	if (value == nullptr)
		return 0;

	return strlen(value);
}

}
}
