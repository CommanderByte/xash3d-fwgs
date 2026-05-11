#ifndef XASH_ENGINE_SERVER_GAME_DLL_CHANGELEVEL_POLICY_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_CHANGELEVEL_POLICY_HPP

#include <string>

namespace xash
{
namespace engine
{
namespace server
{

enum GameDllMapValidationFlags
{
	kGameDllMapExists = 1 << 0,
	kGameDllMapInvalidVersion = 1 << 1,
	kGameDllMapHasLandmark = 1 << 2,
};

enum class GameDllChangeLevelRequestAction
{
	IgnoreEmptyOrInactive,
	IgnoreDuplicateSpawncount,
	QueueChangeLevel,
};

enum class GameDllQueuedChangeLevelAction
{
	RejectInvalidVersion,
	RejectMissingMap,
	RejectSameMap,
	RejectEarlyFrameLoop,
	QueueClassic,
	QueueSmooth,
};

enum class GameDllEntityPatchWriteAction
{
	RejectOpenFailure,
	WritePatch,
};

struct GameDllChangeLevelRequestPlan
{
	GameDllChangeLevelRequestAction action;
	unsigned int nextLastSpawncount;
	std::string level;
	std::string landmark;
};

struct GameDllQueuedChangeLevelPlan
{
	GameDllQueuedChangeLevelAction action;
	std::string mapName;
	std::string landmark;
	bool smoothRequested;
	bool smoothQueued;
	bool landmarkWarning;
	bool multiplayerForcedClassic;
};

struct GameDllEntityPatchWritePlan
{
	GameDllEntityPatchWriteAction action;
	std::string path;
	int removedCount;
};

GameDllChangeLevelRequestPlan BuildGameDllChangeLevelRequestPlan(
	const char *level,
	const char *landmark,
	bool serverActive,
	unsigned int spawncount,
	unsigned int lastSpawncount,
	bool truncateLandmarkAtSpace);
GameDllQueuedChangeLevelPlan BuildGameDllQueuedChangeLevelPlan(
	const char *level,
	const char *landmark,
	const char *currentMap,
	unsigned int mapValidationFlags,
	bool validateChangelevel,
	int maxClients,
	int framecount);
GameDllEntityPatchWritePlan BuildGameDllEntityPatchWritePlan(
	const char *level,
	bool fileOpened,
	int removedCount);

const char *GameDllChangeLevelRequestActionName(
	GameDllChangeLevelRequestAction action);
const char *GameDllQueuedChangeLevelActionName(
	GameDllQueuedChangeLevelAction action);
const char *GameDllEntityPatchWriteActionName(
	GameDllEntityPatchWriteAction action);

}
}
}

#endif
