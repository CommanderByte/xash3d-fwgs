#ifndef XASH_ENGINE_SERVER_GAME_DLL_ENTITY_PARSE_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_ENTITY_PARSE_HPP

#include <string>

namespace xash
{
namespace engine
{
namespace server
{

enum class GameDllEntityKeyValueAction
{
	Skip,
	HandleClassname,
	SkipDuplicateClassname,
	StoreDeferred,
};

enum class GameDllEntityParseAction
{
	RejectMissingClassname,
	RejectInvalidEntity,
	ContinueKeyValues,
};

enum class GameDllEntityLoadAction
{
	UsePhysicsOverride,
	ParseTextEntities,
};

enum class GameDllEntitySpawnAction
{
	SpawnAccepted,
	FreeAndInhibit,
	LeaveKillmeEntity,
};

struct GameDllEntityKeyValuePlan
{
	GameDllEntityKeyValueAction action;
	std::string keyName;
	std::string value;
	bool trailingSpacesTrimmed;
};

struct GameDllAngleRewritePlan
{
	bool rewritten;
	std::string keyName;
	std::string value;
};

struct GameDllCustomEntityKeyValuePlan
{
	bool shouldSend;
	std::string className;
	std::string keyName;
	std::string value;
	bool requireHandled;
};

struct GameDllEntityParsePlan
{
	GameDllEntityParseAction action;
	bool shouldFreeDeferredKeyValues;
};

struct GameDllEntitySpawnPlan
{
	GameDllEntitySpawnAction action;
	bool shouldFreeEdict;
	bool shouldIncrementInhibited;
};

GameDllEntityKeyValuePlan BuildGameDllEntityKeyValuePlan(
	const char *keyName,
	const char *value,
	bool worldSkysphere,
	bool classnameAlreadySeen);
GameDllAngleRewritePlan BuildGameDllAngleRewritePlan(
	const char *keyName,
	const char *value,
	float currentPitch,
	float currentRoll);
GameDllCustomEntityKeyValuePlan BuildGameDllCustomEntityKeyValuePlan(
	bool customEntity,
	const char *classname);
GameDllEntityParsePlan BuildGameDllEntityParsePlan(
	bool classnameSeen,
	bool validAfterAllocation,
	bool killMeAfterAllocation);
GameDllEntityLoadAction BuildGameDllEntityLoadAction(
	bool physicsOverrideAvailable,
	bool physicsOverrideHandled);
GameDllEntitySpawnPlan BuildGameDllEntitySpawnPlan(
	int spawnResult,
	bool killMeFlag);

const char *GameDllEntityKeyValueActionName(
	GameDllEntityKeyValueAction action);
const char *GameDllEntityParseActionName(GameDllEntityParseAction action);
const char *GameDllEntityLoadActionName(GameDllEntityLoadAction action);
const char *GameDllEntitySpawnActionName(GameDllEntitySpawnAction action);

}
}
}

#endif
