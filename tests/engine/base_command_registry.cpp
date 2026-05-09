#include <stdlib.h>

#include <string>
#include <vector>

#include "engine/commands/base_command_registry.hpp"

using namespace xash::engine::commands;

class ReferenceBaseCommandRegistry
{
public:
	ReferenceBaseCommandRegistry()
		: m_count(0)
	{
	}

	bool insert(BaseCommandType type, void *handle, const char *name)
	{
		if (!handle || !name || name[0] == '\0')
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

	void *find(BaseCommandType type, const char *name) const
	{
		if (!name || name[0] == '\0')
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

	BaseCommandMatches findAll(const char *name) const
	{
		BaseCommandMatches matches;

		if (!name || name[0] == '\0')
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

	bool remove(BaseCommandType type, const char *name)
	{
		if (!name || name[0] == '\0')
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

	BaseCommandBucketStats stats() const
	{
		BaseCommandBucketStats result;
		size_t minDepth = static_cast<size_t>(-1);

		result.emptyBuckets = 0;

		for (size_t i = 0; i < BaseCommandRegistry::kBucketCount; ++i)
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

	size_t count() const
	{
		return m_count;
	}

private:
	struct Entry
	{
		BaseCommandType type;
		void *handle;
		std::string name;
	};

	typedef std::vector<Entry> Bucket;

	static int compareNames(const char *left, const char *right)
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

	static char toLower(char value)
	{
		if (value >= 'A' && value <= 'Z')
			return static_cast<char>(value - 'A' + 'a');

		return value;
	}

	static size_t bucketIndex(const char *name)
	{
		size_t hashKey = 5381;

		while (*name)
		{
			const unsigned char value = static_cast<unsigned char>(toLower(*name));
			hashKey = (hashKey << 5) + hashKey + (value & 0xDF);
			++name;
		}

		return hashKey & (BaseCommandRegistry::kBucketCount - 1);
	}

	Bucket m_buckets[BaseCommandRegistry::kBucketCount];
	size_t m_count;
};

static bool SameMatches(const BaseCommandMatches &left, const BaseCommandMatches &right)
{
	return left.command == right.command &&
		left.alias == right.alias &&
		left.cvar == right.cvar;
}

static bool SameStats(const BaseCommandBucketStats &left, const BaseCommandBucketStats &right)
{
	return left.bucketCount == right.bucketCount &&
		left.emptyBuckets == right.emptyBuckets &&
		left.occupiedBuckets == right.occupiedBuckets &&
		left.minDepth == right.minDepth &&
		left.maxDepth == right.maxDepth &&
		left.totalEntries == right.totalEntries;
}

static bool CompareRegistries(const BaseCommandRegistry &modern,
	const ReferenceBaseCommandRegistry &reference)
{
	static const BaseCommandType types[] = {
		BaseCommandType::Command,
		BaseCommandType::Alias,
		BaseCommandType::Cvar,
	};

	static const char *probes[] = {
		"alpha",
		"ALPHA",
		"Beta",
		"gamma",
		"duplicate",
		"DUPLICATE",
		"map",
		"sv_cheats",
		"wait",
		"zeta",
		"missing",
	};

	if (modern.count() != reference.count())
		return false;

	if (!SameStats(modern.stats(), reference.stats()))
		return false;

	for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); ++i)
	{
		if (!SameMatches(modern.findAll(probes[i]), reference.findAll(probes[i])))
			return false;

		for (size_t j = 0; j < sizeof(types) / sizeof(types[0]); ++j)
		{
			if (modern.find(types[j], probes[i]) != reference.find(types[j], probes[i]))
				return false;
		}
	}

	return true;
}

static bool TestTypedCaseInsensitiveLookup()
{
	BaseCommandRegistry registry;
	int command = 1;
	int cvar = 2;

	if (!registry.insert(BaseCommandType::Command, &command, "Map"))
		return false;

	if (!registry.insert(BaseCommandType::Cvar, &cvar, "developer"))
		return false;

	return registry.find(BaseCommandType::Command, "map") == &command &&
		registry.find(BaseCommandType::Command, "MAP") == &command &&
		registry.find(BaseCommandType::Cvar, "Developer") == &cvar &&
		registry.find(BaseCommandType::Alias, "map") == nullptr;
}

static bool TestSharedNameFindAllAndRemove()
{
	BaseCommandRegistry registry;
	int command = 1;
	int alias = 2;
	int cvar = 3;

	if (!registry.insert(BaseCommandType::Cvar, &cvar, "gamma"))
		return false;

	if (!registry.insert(BaseCommandType::Command, &command, "Gamma"))
		return false;

	if (!registry.insert(BaseCommandType::Alias, &alias, "GAMMA"))
		return false;

	BaseCommandMatches matches = registry.findAll("gamma");
	if (matches.command != &command || matches.alias != &alias || matches.cvar != &cvar)
		return false;

	if (!registry.remove(BaseCommandType::Command, "gamma"))
		return false;

	matches = registry.findAll("gamma");
	return matches.command == nullptr && matches.alias == &alias &&
		matches.cvar == &cvar && registry.count() == 2;
}

static bool TestDuplicateLegacyOrdering()
{
	BaseCommandRegistry registry;
	int first = 1;
	int second = 2;

	if (!registry.insert(BaseCommandType::Command, &first, "duplicate"))
		return false;

	if (!registry.insert(BaseCommandType::Command, &second, "DUPLICATE"))
		return false;

	if (registry.find(BaseCommandType::Command, "duplicate") != &second)
		return false;

	BaseCommandMatches matches = registry.findAll("duplicate");
	return matches.command == &first;
}

static bool TestStatsAndClear()
{
	BaseCommandRegistry registry;
	int command = 1;
	int alias = 2;
	int cvar = 3;

	if (!registry.insert(BaseCommandType::Command, &command, "alpha") ||
		!registry.insert(BaseCommandType::Alias, &alias, "beta") ||
		!registry.insert(BaseCommandType::Cvar, &cvar, "gamma"))
	{
		return false;
	}

	BaseCommandBucketStats stats = registry.stats();
	if (stats.bucketCount != BaseCommandRegistry::kBucketCount ||
		stats.totalEntries != 3 || stats.occupiedBuckets == 0 ||
		stats.emptyBuckets == BaseCommandRegistry::kBucketCount ||
		stats.maxDepth == 0)
	{
		return false;
	}

	registry.clear();
	stats = registry.stats();
	return registry.count() == 0 && stats.totalEntries == 0 &&
		stats.emptyBuckets == BaseCommandRegistry::kBucketCount &&
		stats.occupiedBuckets == 0 && stats.minDepth == 0 &&
		stats.maxDepth == 0;
}

static bool TestInvalidAndMissingOperations()
{
	BaseCommandRegistry registry;
	int command = 1;

	if (registry.insert(BaseCommandType::Command, nullptr, "bad"))
		return false;

	if (registry.insert(BaseCommandType::Command, &command, nullptr))
		return false;

	if (registry.insert(BaseCommandType::Command, &command, ""))
		return false;

	if (registry.find(BaseCommandType::Command, "missing"))
		return false;

	return !registry.remove(BaseCommandType::Command, "missing");
}

static bool TestShadowOperationSequence()
{
	BaseCommandRegistry modern;
	ReferenceBaseCommandRegistry reference;

	enum class OperationType
	{
		Insert,
		Remove,
	};

	struct Operation
	{
		OperationType operation;
		BaseCommandType type;
		int handleIndex;
		const char *name;
	};

	int handles[16] = {};

	static const Operation operations[] = {
		{ OperationType::Insert, BaseCommandType::Command, 0, "map" },
		{ OperationType::Insert, BaseCommandType::Alias, 1, "map" },
		{ OperationType::Insert, BaseCommandType::Cvar, 2, "MAP" },
		{ OperationType::Insert, BaseCommandType::Command, 3, "alpha" },
		{ OperationType::Insert, BaseCommandType::Command, 4, "zeta" },
		{ OperationType::Insert, BaseCommandType::Alias, 5, "Beta" },
		{ OperationType::Insert, BaseCommandType::Cvar, 6, "sv_cheats" },
		{ OperationType::Insert, BaseCommandType::Command, 7, "duplicate" },
		{ OperationType::Insert, BaseCommandType::Command, 8, "DUPLICATE" },
		{ OperationType::Insert, BaseCommandType::Command, 9, "duplicate" },
		{ OperationType::Remove, BaseCommandType::Alias, -1, "MAP" },
		{ OperationType::Remove, BaseCommandType::Command, -1, "duplicate" },
		{ OperationType::Insert, BaseCommandType::Alias, 10, "wait" },
		{ OperationType::Insert, BaseCommandType::Command, 11, "wait" },
		{ OperationType::Remove, BaseCommandType::Cvar, -1, "missing" },
		{ OperationType::Remove, BaseCommandType::Command, -1, "alpha" },
		{ OperationType::Insert, BaseCommandType::Cvar, 12, "gamma" },
		{ OperationType::Remove, BaseCommandType::Command, -1, "duplicate" },
		{ OperationType::Remove, BaseCommandType::Command, -1, "duplicate" },
		{ OperationType::Remove, BaseCommandType::Cvar, -1, "map" },
	};

	for (size_t i = 0; i < sizeof(handles) / sizeof(handles[0]); ++i)
		handles[i] = static_cast<int>(i + 1);

	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); ++i)
	{
		const Operation &operation = operations[i];
		bool modernResult;
		bool referenceResult;

		if (operation.operation == OperationType::Insert)
		{
			modernResult = modern.insert(operation.type, &handles[operation.handleIndex],
				operation.name);
			referenceResult = reference.insert(operation.type, &handles[operation.handleIndex],
				operation.name);
		}
		else
		{
			modernResult = modern.remove(operation.type, operation.name);
			referenceResult = reference.remove(operation.type, operation.name);
		}

		if (modernResult != referenceResult)
			return false;

		if (!CompareRegistries(modern, reference))
			return false;
	}

	return true;
}

int main()
{
	if (!TestTypedCaseInsensitiveLookup() ||
		!TestSharedNameFindAllAndRemove() ||
		!TestDuplicateLegacyOrdering() ||
		!TestStatsAndClear() ||
		!TestInvalidAndMissingOperations() ||
		!TestShadowOperationSequence())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
