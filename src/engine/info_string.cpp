#include "engine/info_string.hpp"

#include <cstdio>
#include <cstring>

namespace xash
{
namespace engine
{

namespace
{

char ToLower(char value)
{
	if (value >= 'A' && value <= 'Z')
		return static_cast<char>(value - 'A' + 'a');

	return value;
}

bool StringEmptyOrNull(const char *value)
{
	return !value || value[0] == '\0';
}

int StringCompareInsensitive(const char *left, const char *right)
{
	while (*left && *right)
	{
		const char leftChar = ToLower(*left);
		const char rightChar = ToLower(*right);

		if (leftChar < rightChar)
			return -1;

		if (leftChar > rightChar)
			return 1;

		++left;
		++right;
	}

	if (*left == *right)
		return 0;

	return *left ? 1 : -1;
}

void CopyString(char *dst, std::size_t size, const char *src)
{
	if (size == 0)
		return;

	if (!src)
	{
		dst[0] = '\0';
		return;
	}

	std::size_t i = 0;
	for (; i + 1 < size && src[i]; ++i)
		dst[i] = src[i];

	dst[i] = '\0';
}

bool Contains(const char *value, char needle)
{
	return value && std::strchr(value, needle) != nullptr;
}

bool ContainsString(const char *value, const char *needle)
{
	return value && std::strstr(value, needle) != nullptr;
}

char *FindLargestKey(char *info)
{
	static char largestKey[kInfoStringMaxKeyValueSize];
	int largestSize = 0;
	InfoStringIterator iterator(info);
	InfoStringPair pair;

	largestKey[0] = '\0';

	while (iterator.next(pair))
	{
		if (!pair.hasValue)
			return largestKey;

		const int size = static_cast<int>(std::strlen(pair.key) + std::strlen(pair.value));

		if (size > largestSize && !InfoStringIsKeyImportant(pair.key))
		{
			CopyString(largestKey, sizeof(largestKey), pair.key);
			largestSize = size;
		}
	}

	return largestKey;
}

}

InfoStringSetResult::InfoStringSetResult()
	: ok(true)
	, error(InfoStringSetError::None)
{
}

InfoStringSetResult::InfoStringSetResult(bool okValue, InfoStringSetError errorValue)
	: ok(okValue)
	, error(errorValue)
{
}

InfoStringPair::InfoStringPair()
	: hasValue(false)
{
	key[0] = '\0';
	value[0] = '\0';
}

InfoStringIterator::InfoStringIterator(const char *info)
	: m_cursor(info ? info : "")
{
	if (*m_cursor == '\\')
		++m_cursor;
}

bool InfoStringIterator::next(InfoStringPair &pair)
{
	pair = InfoStringPair();

	if (!*m_cursor)
		return false;

	std::size_t count = 0;
	char *out = pair.key;

	while (count < (kInfoStringMaxKeyValueSize - 1) && *m_cursor && *m_cursor != '\\')
	{
		*out++ = *m_cursor++;
		++count;
	}
	*out = '\0';

	if (!*m_cursor)
		return true;

	++m_cursor;
	count = 0;
	out = pair.value;

	while (count < (kInfoStringMaxKeyValueSize - 1) && *m_cursor && *m_cursor != '\\')
	{
		*out++ = *m_cursor++;
		++count;
	}
	*out = '\0';
	pair.hasValue = true;

	if (*m_cursor)
		++m_cursor;

	return true;
}

InfoStringLookupBuffers::InfoStringLookupBuffers()
	: m_index(0)
{
	for (std::size_t i = 0; i < 4; ++i)
		m_values[i][0] = '\0';
}

char *InfoStringLookupBuffers::next()
{
	m_index = (m_index + 1) % 4;
	m_values[m_index][0] = '\0';
	return m_values[m_index];
}

const char *InfoStringValueForKey(const char *info, const char *key,
	InfoStringLookupBuffers &buffers)
{
	InfoStringIterator iterator(info);
	InfoStringPair pair;
	char *value = buffers.next();

	while (iterator.next(pair))
	{
		if (!pair.hasValue)
			return "";

		if (std::strcmp(key, pair.key) == 0)
		{
			CopyString(value, kInfoStringMaxKeyValueSize, pair.value);
			return value;
		}
	}

	return "";
}

bool InfoStringIsValid(const char *info)
{
	InfoStringIterator iterator(info);
	InfoStringPair pair;

	while (iterator.next(pair))
	{
		if (!pair.hasValue)
			return false;

		if (StringEmptyOrNull(pair.value))
			return false;
	}

	return true;
}

bool InfoStringRemoveKey(char *info, const char *key)
{
	char *cursor = info;
	const int compareSize = static_cast<int>(
		std::strlen(key) > (kInfoStringMaxKeyValueSize - 1) ?
			(kInfoStringMaxKeyValueSize - 1) : std::strlen(key));

	if (Contains(key, '\\'))
		return false;

	while (true)
	{
		char *start = cursor;
		char parsedKey[kInfoStringMaxKeyValueSize];
		char parsedValue[kInfoStringMaxKeyValueSize];
		int count = 0;
		char *out = parsedKey;

		if (*cursor == '\\')
			++cursor;

		while (count < static_cast<int>(kInfoStringMaxKeyValueSize - 1) && *cursor != '\\')
		{
			if (!*cursor)
				return false;

			*out++ = *cursor++;
			++count;
		}
		*out = '\0';
		++cursor;

		count = 0;
		out = parsedValue;
		while (count < static_cast<int>(kInfoStringMaxKeyValueSize - 1) &&
			*cursor != '\\' && *cursor)
		{
			if (!*cursor)
				return false;

			*out++ = *cursor++;
			++count;
		}
		*out = '\0';

		if (std::strncmp(key, parsedKey, compareSize) == 0)
		{
			const std::size_t size = std::strlen(cursor) + 1;
			std::memmove(start, cursor, size);
			return true;
		}

		if (!*cursor)
			return false;
	}
}

void InfoStringRemovePrefixedKeys(char *info, char prefix)
{
	char *cursor = info;

	while (true)
	{
		char parsedKey[kInfoStringMaxKeyValueSize];
		char parsedValue[kInfoStringMaxKeyValueSize];
		int count = 0;
		char *out = parsedKey;

		if (*cursor == '\\')
			++cursor;

		while (count < static_cast<int>(kInfoStringMaxKeyValueSize - 1) && *cursor != '\\')
		{
			if (!*cursor)
				return;

			*out++ = *cursor++;
			++count;
		}
		*out = '\0';
		++cursor;

		count = 0;
		out = parsedValue;
		while (count < static_cast<int>(kInfoStringMaxKeyValueSize - 1) &&
			*cursor && *cursor != '\\')
		{
			if (!*cursor)
				return;

			*out++ = *cursor++;
			++count;
		}
		*out = '\0';

		if (parsedKey[0] == prefix)
		{
			InfoStringRemoveKey(info, parsedKey);
			cursor = info;
		}

		if (!*cursor)
			return;
	}
}

InfoStringSetResult InfoStringSetValueForStarKey(char *info, const char *key,
	const char *value, int maxSize)
{
	if (Contains(key, '\\') || Contains(value, '\\'))
		return InfoStringSetResult(false, InfoStringSetError::Backslash);

	if (ContainsString(key, "..") || ContainsString(value, ".."))
		return InfoStringSetResult(false, InfoStringSetError::DotDot);

	if (Contains(key, '"') || Contains(value, '"'))
		return InfoStringSetResult(false, InfoStringSetError::Quote);

	if (std::strlen(key) > (kInfoStringMaxKeyValueSize - 1) ||
		std::strlen(value) > (kInfoStringMaxKeyValueSize - 1))
	{
		return InfoStringSetResult(false, InfoStringSetError::TooLong);
	}

	InfoStringRemoveKey(info, key);

	if (StringEmptyOrNull(value))
		return InfoStringSetResult();

	char pending[1024];
	std::snprintf(pending, sizeof(pending), "\\%s\\%s", key, value);

	if (static_cast<int>(std::strlen(pending) + std::strlen(info)) > maxSize)
	{
		if (InfoStringIsKeyImportant(key))
		{
			char *largestKey;

			do
			{
				largestKey = FindLargestKey(info);
				InfoStringRemoveKey(info, largestKey);
			}
			while ((static_cast<int>(std::strlen(pending) + std::strlen(info)) >= maxSize) &&
				*largestKey != '\0');

			if (largestKey[0] == '\0')
				return InfoStringSetResult();
		}
		else
		{
			return InfoStringSetResult();
		}
	}

	char *write = info + std::strlen(info);
	const char *read = pending;
	const bool team = StringCompareInsensitive(key, "team") == 0;

	while (*read)
	{
		int valueByte = static_cast<unsigned char>(*read++);

		if (team)
			valueByte = ToLower(static_cast<char>(valueByte));

		if (valueByte > 13)
			*write++ = static_cast<char>(valueByte);
	}
	*write = '\0';

	return InfoStringSetResult();
}

InfoStringSetResult InfoStringSetValueForKey(char *info, const char *key,
	const char *value, int maxSize)
{
	if (key[0] == '*')
		return InfoStringSetResult(false, InfoStringSetError::StarKey);

	return InfoStringSetValueForStarKey(info, key, value, maxSize);
}

bool InfoStringIsKeyImportant(const char *key)
{
	if (key[0] == '*')
		return true;
	if (std::strcmp(key, "name") == 0)
		return true;
	if (std::strcmp(key, "model") == 0)
		return true;
	if (std::strcmp(key, "rate") == 0)
		return true;
	if (std::strcmp(key, "topcolor") == 0)
		return true;
	if (std::strcmp(key, "bottomcolor") == 0)
		return true;
	if (std::strcmp(key, "cl_updaterate") == 0)
		return true;
	if (std::strcmp(key, "cl_lw") == 0)
		return true;
	if (std::strcmp(key, "cl_lc") == 0)
		return true;
	if (std::strcmp(key, "cl_nopred") == 0)
		return true;

	return false;
}

}
}
