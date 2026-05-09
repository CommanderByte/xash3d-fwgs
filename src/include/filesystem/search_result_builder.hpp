#ifndef XASH_FILESYSTEM_SEARCH_RESULT_BUILDER_HPP
#define XASH_FILESYSTEM_SEARCH_RESULT_BUILDER_HPP

#include <stddef.h>

namespace xash
{
namespace filesystem
{

class SearchResultBuilder
{
public:
	static void sort(char **strings, size_t count);
	static size_t compactDuplicates(char **strings, size_t count);
	static size_t packedStringBytes(char *const *strings, size_t count);
	static void copyPacked(
		char *const *strings,
		size_t count,
		char **filenames,
		char *buffer);

private:
	static int compare(const char *a, const char *b);
	static size_t length(const char *value);
};

}
}

#endif
