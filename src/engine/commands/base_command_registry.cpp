#include "engine/commands/base_command_registry.hpp"

#include <limits>

namespace xash
{
namespace engine
{
namespace commands
{

BaseCommandMatches::BaseCommandMatches()
	: command(nullptr)
	, alias(nullptr)
	, cvar(nullptr)
{
}

BaseCommandBucketStats::BaseCommandBucketStats()
	: bucketCount(BaseCommandRegistry::kBucketCount)
	, emptyBuckets(BaseCommandRegistry::kBucketCount)
	, occupiedBuckets(0)
	, minDepth(0)
	, maxDepth(0)
	, totalEntries(0)
{
}

BaseCommandRegistry::BaseCommandRegistry()
	: m_count(0)
{
}

bool BaseCommandRegistry::insert(BaseCommandType type, void *handle, const char *name)
{
	if (!handle || !validName(name))
		return false;

	Entry entry;
	entry.type = type;
	entry.handle = handle;
	entry.name = name;

	Bucket &bucket = m_buckets[bucketIndex(name)];
	Bucket::iterator insertAt = bucket.begin();

	while (insertAt != bucket.end() && compareNames(insertAt->name.c_str(), name) < 0)
		++insertAt;

	bucket.insert(insertAt, entry);
	++m_count;
	return true;
}

void *BaseCommandRegistry::find(BaseCommandType type, const char *name) const
{
	if (!validName(name))
		return nullptr;

	const Bucket &bucket = m_buckets[bucketIndex(name)];

	for (Bucket::const_iterator it = bucket.begin(); it != bucket.end(); ++it)
	{
		const int comparison = compareNames(it->name.c_str(), name);

		if (comparison > 0)
			break;

		if (comparison == 0 && it->type == type)
			return it->handle;
	}

	return nullptr;
}

BaseCommandMatches BaseCommandRegistry::findAll(const char *name) const
{
	BaseCommandMatches matches;

	if (!validName(name))
		return matches;

	const Bucket &bucket = m_buckets[bucketIndex(name)];

	for (Bucket::const_iterator it = bucket.begin(); it != bucket.end(); ++it)
	{
		const int comparison = compareNames(it->name.c_str(), name);

		if (comparison > 0)
			break;

		if (comparison != 0)
			continue;

		switch (it->type)
		{
		case BaseCommandType::Command:
			matches.command = it->handle;
			break;
		case BaseCommandType::Alias:
			matches.alias = it->handle;
			break;
		case BaseCommandType::Cvar:
			matches.cvar = it->handle;
			break;
		default:
			break;
		}
	}

	return matches;
}

bool BaseCommandRegistry::remove(BaseCommandType type, const char *name)
{
	if (!validName(name))
		return false;

	Bucket &bucket = m_buckets[bucketIndex(name)];

	for (Bucket::iterator it = bucket.begin(); it != bucket.end(); ++it)
	{
		const int comparison = compareNames(it->name.c_str(), name);

		if (comparison > 0)
			break;

		if (comparison == 0 && it->type == type)
		{
			bucket.erase(it);
			--m_count;
			return true;
		}
	}

	return false;
}

void BaseCommandRegistry::clear()
{
	for (size_t i = 0; i < kBucketCount; ++i)
		m_buckets[i].clear();

	m_count = 0;
}

size_t BaseCommandRegistry::count() const
{
	return m_count;
}

BaseCommandBucketStats BaseCommandRegistry::stats() const
{
	BaseCommandBucketStats result;
	size_t minDepth = std::numeric_limits<size_t>::max();

	result.emptyBuckets = 0;

	for (size_t i = 0; i < kBucketCount; ++i)
	{
		const size_t depth = m_buckets[i].size();
		result.totalEntries += depth;

		if (depth == 0)
		{
			++result.emptyBuckets;
			continue;
		}

		++result.occupiedBuckets;

		if (depth < minDepth)
			minDepth = depth;

		if (depth > result.maxDepth)
			result.maxDepth = depth;
	}

	if (result.occupiedBuckets != 0)
		result.minDepth = minDepth;

	return result;
}

int BaseCommandRegistry::compareNames(const char *left, const char *right)
{
	while (*left && *right)
	{
		const char leftChar = toLower(*left);
		const char rightChar = toLower(*right);

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

char BaseCommandRegistry::toLower(char value)
{
	if (value >= 'A' && value <= 'Z')
		return static_cast<char>(value - 'A' + 'a');

	return value;
}

bool BaseCommandRegistry::validName(const char *name)
{
	return name && name[0] != '\0';
}

size_t BaseCommandRegistry::bucketIndex(const char *name) const
{
	size_t hashKey = 5381;

	while (*name)
	{
		const unsigned char value = static_cast<unsigned char>(toLower(*name));
		hashKey = (hashKey << 5) + hashKey + (value & 0xDF);
		++name;
	}

	return hashKey & (kBucketCount - 1);
}

}
}
}
