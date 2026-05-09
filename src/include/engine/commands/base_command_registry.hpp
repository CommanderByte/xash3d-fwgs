#ifndef XASH_ENGINE_COMMANDS_BASE_COMMAND_REGISTRY_HPP
#define XASH_ENGINE_COMMANDS_BASE_COMMAND_REGISTRY_HPP

#include <stddef.h>

#include <string>
#include <vector>

namespace xash
{
namespace engine
{
namespace commands
{

enum class BaseCommandType
{
	DontCare = 0,
	Cvar,
	Command,
	Alias,
};

struct BaseCommandMatches
{
	BaseCommandMatches();

	void *command;
	void *alias;
	void *cvar;
};

struct BaseCommandBucketStats
{
	BaseCommandBucketStats();

	size_t bucketCount;
	size_t emptyBuckets;
	size_t occupiedBuckets;
	size_t minDepth;
	size_t maxDepth;
	size_t totalEntries;
};

class BaseCommandRegistry
{
public:
	static const size_t kBucketCount = 64;

	BaseCommandRegistry();

	bool insert(BaseCommandType type, void *handle, const char *name);
	void *find(BaseCommandType type, const char *name) const;
	BaseCommandMatches findAll(const char *name) const;
	bool remove(BaseCommandType type, const char *name);
	void clear();

	size_t count() const;
	BaseCommandBucketStats stats() const;

private:
	struct Entry
	{
		BaseCommandType type;
		void *handle;
		std::string name;
	};

	typedef std::vector<Entry> Bucket;

	static int compareNames(const char *left, const char *right);
	static char toLower(char value);
	static bool validName(const char *name);

	size_t bucketIndex(const char *name) const;

	Bucket m_buckets[kBucketCount];
	size_t m_count;
};

}
}
}

#endif
