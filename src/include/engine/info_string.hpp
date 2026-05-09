#ifndef XASH_ENGINE_INFO_STRING_HPP
#define XASH_ENGINE_INFO_STRING_HPP

#include <cstddef>

namespace xash
{
namespace engine
{

static const std::size_t kInfoStringMaxKeyValueSize = 128;

enum class InfoStringSetError
{
	None,
	StarKey,
	Backslash,
	DotDot,
	Quote,
	TooLong,
};

struct InfoStringSetResult
{
	InfoStringSetResult();
	InfoStringSetResult(bool okValue, InfoStringSetError errorValue);

	bool ok;
	InfoStringSetError error;
};

struct InfoStringPair
{
	InfoStringPair();

	char key[kInfoStringMaxKeyValueSize];
	char value[kInfoStringMaxKeyValueSize];
	bool hasValue;
};

class InfoStringIterator
{
public:
	explicit InfoStringIterator(const char *info);

	bool next(InfoStringPair &pair);

private:
	const char *m_cursor;
};

class InfoStringLookupBuffers
{
public:
	InfoStringLookupBuffers();

	char *next();

private:
	char m_values[4][kInfoStringMaxKeyValueSize];
	int m_index;
};

const char *InfoStringValueForKey(const char *info, const char *key,
	InfoStringLookupBuffers &buffers);
bool InfoStringIsValid(const char *info);
bool InfoStringRemoveKey(char *info, const char *key);
void InfoStringRemovePrefixedKeys(char *info, char prefix);
InfoStringSetResult InfoStringSetValueForStarKey(char *info, const char *key,
	const char *value, int maxSize);
InfoStringSetResult InfoStringSetValueForKey(char *info, const char *key,
	const char *value, int maxSize);
bool InfoStringIsKeyImportant(const char *key);

}
}

#endif
