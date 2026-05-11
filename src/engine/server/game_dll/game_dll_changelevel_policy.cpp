#include "engine/server/game_dll/game_dll_changelevel_policy.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

const char *SafeText(const char *text)
{
	return text ? text : "";
}

bool EmptyOrNull(const char *text)
{
	return !text || text[0] == '\0';
}

std::string StripExtension(const char *path)
{
	std::string result = SafeText(path);
	const std::string::size_type slash = result.find_last_of("/\\");
	const std::string::size_type dot = result.find_last_of('.');

	if (dot != std::string::npos &&
		(slash == std::string::npos || dot > slash))
	{
		result.erase(dot);
	}

	return result;
}

std::string TruncateAtSpace(const char *text)
{
	std::string result;
	const char *cursor = SafeText(text);

	while (*cursor && static_cast<unsigned char>(*cursor) != ' ')
	{
		result.push_back(*cursor);
		++cursor;
	}

	return result;
}

bool HasFlag(unsigned int flags, GameDllMapValidationFlags flag)
{
	return (flags & static_cast<unsigned int>(flag)) != 0;
}

}

GameDllChangeLevelRequestPlan BuildGameDllChangeLevelRequestPlan(
	const char *level,
	const char *landmark,
	bool serverActive,
	unsigned int spawncount,
	unsigned int lastSpawncount,
	bool truncateLandmarkAtSpace)
{
	GameDllChangeLevelRequestPlan plan = {};
	plan.nextLastSpawncount = lastSpawncount;

	if (EmptyOrNull(level) || !serverActive)
	{
		plan.action = GameDllChangeLevelRequestAction::IgnoreEmptyOrInactive;
		return plan;
	}

	if (spawncount == lastSpawncount)
	{
		plan.action =
			GameDllChangeLevelRequestAction::IgnoreDuplicateSpawncount;
		return plan;
	}

	plan.action = GameDllChangeLevelRequestAction::QueueChangeLevel;
	plan.nextLastSpawncount = spawncount;
	plan.level = SafeText(level);

	if (!EmptyOrNull(landmark))
	{
		plan.landmark = truncateLandmarkAtSpace ?
			TruncateAtSpace(landmark) :
			SafeText(landmark);
	}

	return plan;
}

GameDllQueuedChangeLevelPlan BuildGameDllQueuedChangeLevelPlan(
	const char *level,
	const char *landmark,
	const char *currentMap,
	unsigned int mapValidationFlags,
	bool validateChangelevel,
	int maxClients,
	int framecount)
{
	GameDllQueuedChangeLevelPlan plan = {};
	plan.mapName = StripExtension(level);
	plan.landmark = SafeText(landmark);
	plan.smoothRequested = !plan.landmark.empty();
	plan.smoothQueued = plan.smoothRequested;

	if (HasFlag(mapValidationFlags, kGameDllMapInvalidVersion))
	{
		plan.action = GameDllQueuedChangeLevelAction::RejectInvalidVersion;
		plan.smoothQueued = false;
		return plan;
	}

	if (!HasFlag(mapValidationFlags, kGameDllMapExists))
	{
		plan.action = GameDllQueuedChangeLevelAction::RejectMissingMap;
		plan.smoothQueued = false;
		return plan;
	}

	if (plan.smoothQueued &&
		!HasFlag(mapValidationFlags, kGameDllMapHasLandmark) &&
		validateChangelevel)
	{
		plan.landmarkWarning = true;
		plan.smoothQueued = false;
	}

	if (maxClients > 1)
	{
		plan.multiplayerForcedClassic = plan.smoothQueued;
		plan.smoothQueued = false;
	}

	if (plan.smoothQueued && std::strcmp(SafeText(currentMap), SafeText(level)) == 0)
	{
		plan.action = GameDllQueuedChangeLevelAction::RejectSameMap;
		plan.smoothQueued = false;
		return plan;
	}

	if (framecount < 15 && validateChangelevel)
	{
		plan.action = GameDllQueuedChangeLevelAction::RejectEarlyFrameLoop;
		plan.smoothQueued = false;
		return plan;
	}

	plan.action = plan.smoothQueued ?
		GameDllQueuedChangeLevelAction::QueueSmooth :
		GameDllQueuedChangeLevelAction::QueueClassic;
	return plan;
}

GameDllEntityPatchWritePlan BuildGameDllEntityPatchWritePlan(
	const char *level,
	bool fileOpened,
	int removedCount)
{
	GameDllEntityPatchWritePlan plan = {};
	plan.path = std::string("save/") + SafeText(level) + ".HL3";
	plan.removedCount = removedCount < 0 ? 0 : removedCount;

	if (!fileOpened)
	{
		plan.action = GameDllEntityPatchWriteAction::RejectOpenFailure;
		return plan;
	}

	plan.action = GameDllEntityPatchWriteAction::WritePatch;
	return plan;
}

const char *GameDllChangeLevelRequestActionName(
	GameDllChangeLevelRequestAction action)
{
	switch (action)
	{
	case GameDllChangeLevelRequestAction::IgnoreEmptyOrInactive:
		return "ignore-empty-or-inactive";
	case GameDllChangeLevelRequestAction::IgnoreDuplicateSpawncount:
		return "ignore-duplicate-spawncount";
	case GameDllChangeLevelRequestAction::QueueChangeLevel:
		return "queue-changelevel";
	}

	return "unknown";
}

const char *GameDllQueuedChangeLevelActionName(
	GameDllQueuedChangeLevelAction action)
{
	switch (action)
	{
	case GameDllQueuedChangeLevelAction::RejectInvalidVersion:
		return "reject-invalid-version";
	case GameDllQueuedChangeLevelAction::RejectMissingMap:
		return "reject-missing-map";
	case GameDllQueuedChangeLevelAction::RejectSameMap:
		return "reject-same-map";
	case GameDllQueuedChangeLevelAction::RejectEarlyFrameLoop:
		return "reject-early-frame-loop";
	case GameDllQueuedChangeLevelAction::QueueClassic:
		return "queue-classic";
	case GameDllQueuedChangeLevelAction::QueueSmooth:
		return "queue-smooth";
	}

	return "unknown";
}

const char *GameDllEntityPatchWriteActionName(
	GameDllEntityPatchWriteAction action)
{
	switch (action)
	{
	case GameDllEntityPatchWriteAction::RejectOpenFailure:
		return "reject-open-failure";
	case GameDllEntityPatchWriteAction::WritePatch:
		return "write-patch";
	}

	return "unknown";
}

}
}
}
