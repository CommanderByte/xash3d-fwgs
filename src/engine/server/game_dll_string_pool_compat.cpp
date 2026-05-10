#include "engine/server/game_dll_string_pool_compat.hpp"

#include <climits>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

constexpr std::size_t kFirstStringOffset = 1;

const char *SafeText(const char *text)
{
	return text ? text : "";
}

}

GameDllProcessedString ProcessGameDllString(const char *text)
{
	GameDllProcessedString processed = {};
	const char *cursor = SafeText(text);

	while (*cursor)
	{
		if (*cursor == '\\')
		{
			char replacement = '\0';

			switch (cursor[1])
			{
			case 'n':
				replacement = '\n';
				break;
			case 'r':
				replacement = '\r';
				break;
			case 't':
				replacement = '\t';
				break;
			default:
				break;
			}

			if (replacement)
			{
				processed.value.push_back(replacement);
				cursor += 2;
				continue;
			}
		}

		processed.value.push_back(*cursor);
		++cursor;
	}

	processed.requiredLength = processed.value.size() + 1;
	return processed;
}

GameDllStringOverrideAction BuildGameDllStringOverrideAction(
	bool overrideAvailable)
{
	return overrideAvailable ?
		GameDllStringOverrideAction::UsePhysicsOverride :
		GameDllStringOverrideAction::UseEnginePool;
}

GameDllMakeStringDecision BuildGameDllMakeStringDecision(
	long long pointerDifference,
	int minHandle,
	int maxHandle)
{
	GameDllMakeStringDecision decision = {};

	if (pointerDifference > maxHandle || pointerDifference < minHandle)
	{
		decision.action = GameDllMakeStringAction::AllocateFallback;
		return decision;
	}

	decision.action = GameDllMakeStringAction::UseExistingOffset;
	decision.handle = static_cast<int>(pointerDifference);
	return decision;
}

GameDllStringPoolCompatibilityModel::GameDllStringPoolCompatibilityModel(
	std::size_t maxStringArray,
	bool allowDuplicates)
	: m_maxStringArray(maxStringArray),
	  m_allowDuplicates(allowDuplicates),
	  m_dynamicMode(false),
	  m_usedBytes(kFirstStringOffset),
	  m_maxAlloc(0),
	  m_totalAlloc(0),
	  m_duplicateCount(0),
	  m_overflowCount(0)
{
}

void GameDllStringPoolCompatibilityModel::setDynamicMode(bool dynamicMode)
{
	if (m_dynamicMode == dynamicMode)
		return;

	m_dynamicMode = dynamicMode;
	clear(false);
}

void GameDllStringPoolCompatibilityModel::clear(bool clearStats)
{
	clearActiveEntries();
	m_usedBytes = kFirstStringOffset;

	if (clearStats)
	{
		m_maxAlloc = 0;
		m_totalAlloc = 0;
		m_duplicateCount = 0;
		m_overflowCount = 0;
	}
}

GameDllStringAllocation GameDllStringPoolCompatibilityModel::allocate(
	const char *text)
{
	GameDllStringAllocation allocation = {};
	const GameDllProcessedString processed = ProcessGameDllString(text);

	allocation.value = processed.value;

	if (!m_allowDuplicates)
	{
		for (const Entry &entry : m_entries)
		{
			if (entry.value == processed.value)
			{
				++m_duplicateCount;
				allocation.handle = entry.handle;
				allocation.duplicate = true;
				return allocation;
			}
		}
	}

	if (m_usedBytes + processed.requiredLength + 1 > m_maxStringArray)
	{
		clearActiveEntries();
		m_usedBytes = kFirstStringOffset;
		++m_overflowCount;
		allocation.overflowed = true;
	}

	allocation.handle = static_cast<int>(m_usedBytes);
	m_entries.push_back(Entry{ allocation.handle, processed.value });
	m_usedBytes += processed.requiredLength;
	m_totalAlloc += processed.requiredLength;

	const std::size_t absoluteOffset =
		currentBaseOffset() + static_cast<std::size_t>(allocation.handle);
	if (absoluteOffset > m_maxAlloc)
		m_maxAlloc = absoluteOffset;

	return allocation;
}

const char *GameDllStringPoolCompatibilityModel::getString(int handle) const
{
	if (handle == 0)
		return "";

	for (const Entry &entry : m_entries)
	{
		if (entry.handle == handle)
			return entry.value.c_str();
	}

	return nullptr;
}

bool GameDllStringPoolCompatibilityModel::isValidHandle(int handle) const
{
	return getString(handle) != nullptr;
}

GameDllStringPoolStats GameDllStringPoolCompatibilityModel::stats() const
{
	GameDllStringPoolStats result = {};
	result.maxStringArray = m_maxStringArray;
	result.maxAlloc = m_maxAlloc;
	result.totalAlloc = m_totalAlloc;
	result.duplicateCount = m_duplicateCount;
	result.overflowCount = m_overflowCount;
	result.allowDuplicates = m_allowDuplicates;
	result.dynamicMode = m_dynamicMode;
	return result;
}

std::size_t GameDllStringPoolCompatibilityModel::currentBaseOffset() const
{
	return m_dynamicMode ? 0 : m_maxStringArray;
}

void GameDllStringPoolCompatibilityModel::clearActiveEntries()
{
	m_entries.clear();
}

const char *GameDllStringOverrideActionName(GameDllStringOverrideAction action)
{
	switch (action)
	{
	case GameDllStringOverrideAction::UseEnginePool:
		return "use-engine-pool";
	case GameDllStringOverrideAction::UsePhysicsOverride:
		return "use-physics-override";
	}

	return "unknown";
}

const char *GameDllMakeStringActionName(GameDllMakeStringAction action)
{
	switch (action)
	{
	case GameDllMakeStringAction::UseExistingOffset:
		return "use-existing-offset";
	case GameDllMakeStringAction::AllocateFallback:
		return "allocate-fallback";
	}

	return "unknown";
}

}
}
}
