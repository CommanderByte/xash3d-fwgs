#ifndef XASH_ENGINE_SERVER_GAME_DLL_STRING_POOL_COMPAT_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_STRING_POOL_COMPAT_HPP

#include <cstddef>
#include <string>
#include <deque>

namespace xash
{
namespace engine
{
namespace server
{

enum class GameDllStringOverrideAction
{
	UseEnginePool,
	UsePhysicsOverride,
};

enum class GameDllMakeStringAction
{
	UseExistingOffset,
	AllocateFallback,
};

struct GameDllProcessedString
{
	std::string value;
	std::size_t requiredLength;
};

struct GameDllStringPoolStats
{
	std::size_t maxStringArray;
	std::size_t maxAlloc;
	std::size_t totalAlloc;
	std::size_t duplicateCount;
	std::size_t overflowCount;
	bool allowDuplicates;
	bool dynamicMode;
};

struct GameDllStringAllocation
{
	int handle;
	bool duplicate;
	bool overflowed;
	std::string value;
};

struct GameDllMakeStringDecision
{
	GameDllMakeStringAction action;
	int handle;
};

GameDllProcessedString ProcessGameDllString(const char *text);
GameDllStringOverrideAction BuildGameDllStringOverrideAction(
	bool overrideAvailable);
GameDllMakeStringDecision BuildGameDllMakeStringDecision(
	long long pointerDifference,
	int minHandle,
	int maxHandle);

class GameDllStringPoolCompatibilityModel
{
public:
	GameDllStringPoolCompatibilityModel(
		std::size_t maxStringArray,
		bool allowDuplicates);

	void setDynamicMode(bool dynamicMode);
	void clear(bool clearStats);

	GameDllStringAllocation allocate(const char *text);
	const char *getString(int handle) const;
	bool isValidHandle(int handle) const;

	GameDllStringPoolStats stats() const;

private:
	struct Entry
	{
		int handle;
		std::string value;
	};

	std::size_t currentBaseOffset() const;
	void clearActiveEntries();

	std::size_t m_maxStringArray;
	bool m_allowDuplicates;
	bool m_dynamicMode;
	std::size_t m_usedBytes;
	std::size_t m_maxAlloc;
	std::size_t m_totalAlloc;
	std::size_t m_duplicateCount;
	std::size_t m_overflowCount;
	std::deque<Entry> m_entries;
};

const char *GameDllStringOverrideActionName(GameDllStringOverrideAction action);
const char *GameDllMakeStringActionName(GameDllMakeStringAction action);

}
}
}

#endif
