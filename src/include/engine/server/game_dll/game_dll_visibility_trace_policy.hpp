#ifndef XASH_ENGINE_SERVER_GAME_DLL_VISIBILITY_TRACE_POLICY_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_VISIBILITY_TRACE_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

enum class GameDllTraceEntityAction
{
	UseWorldEntity,
	UseTraceEntity,
};

enum class GameDllTraceCallAction
{
	SkipInvalidEntity,
	RunTrace,
};

enum class GameDllTraceModelAction
{
	SkipInvalidEntity,
	RunCustomClip,
	RunBrushWithTemporaryBsp,
	RunEntityClip,
};

enum class GameDllTraceTextureAction
{
	ReturnNull,
	TraceTexture,
};

enum class GameDllVisibilityCheckAction
{
	ReturnInvisible,
	ReturnVisibleFull,
	CheckLeafList,
	UpcastOwnerAndCheck,
	CheckLeafListThenHeadnode,
};

enum class GameDllClientPvsAction
{
	ReturnWorldEntity,
	UseCachedClientPvs,
	RefreshClientPvs,
};

struct GameDllTraceHullPlan
{
	int hullNumber;
	bool clamped;
};

struct GameDllTraceMonsterHullPlan
{
	GameDllTraceCallAction action;
	bool monsterClip;
};

struct GameDllTraceModelPlan
{
	GameDllTraceModelAction action;
	int hullNumber;
	bool clampedHull;
};

struct GameDllFatVisibilityPlan
{
	bool fullVisibility;
	bool mergeVisibility;
	bool passiveAudioSet;
};

struct GameDllVisibilityCheckPlan
{
	GameDllVisibilityCheckAction action;
	int leafCapacity;
	bool largeLeafs;
};

struct GameDllVisibilityHeadnodeResult
{
	int visibilityCode;
	bool shouldCacheLeaf;
	int nextLeafCount;
};

struct GameDllClientPvsPlan
{
	GameDllClientPvsAction action;
	bool mergePortalVisibility;
};

GameDllTraceEntityAction BuildGameDllTraceEntityAction(bool validTraceEntity);
GameDllTraceCallAction BuildGameDllTraceTossAction(bool validEntity);
GameDllTraceHullPlan BuildGameDllTraceHullPlan(int hullNumber);
GameDllTraceMonsterHullPlan BuildGameDllTraceMonsterHullPlan(
	bool validEntity,
	bool monsterClipFlag);
bool BuildGameDllTraceMonsterHullResult(bool allSolid, float fraction);
GameDllTraceModelPlan BuildGameDllTraceModelPlan(
	bool validEntity,
	int hullNumber,
	bool solidCustom,
	bool modelIsBrush);
GameDllTraceTextureAction BuildGameDllTraceTextureAction(bool validEntity);
GameDllFatVisibilityPlan BuildGameDllFatVisibilityPlan(
	bool hasVisData,
	bool svNoVis,
	bool originPresent,
	bool clientVisibilityDisabled,
	bool hostMergeVisibility,
	bool passiveAudioSet);
GameDllVisibilityCheckPlan BuildGameDllVisibilityCheckPlan(
	bool validEntity,
	bool hasVisibilitySet,
	bool customEntity,
	bool hasOwner,
	bool ownerIsClient,
	int headnode,
	bool largeLeafs);
int BuildGameDllVisibilityLeafResult(bool anyLeafVisible);
GameDllVisibilityHeadnodeResult BuildGameDllVisibilityHeadnodeResult(
	bool headnodeVisible,
	int currentLeafCount,
	bool largeLeafs);
GameDllClientPvsPlan BuildGameDllClientPvsPlan(
	bool validViewer,
	bool viewerIsMonster,
	float secondsSinceLastCheck);
GameDllTraceEntityAction BuildGameDllClientPvsResult(
	bool hasSpawnedClient,
	bool visible);
bool BuildGameDllCanSkipPlayerResult(bool hasClient, bool localWeapons);

const char *GameDllTraceEntityActionName(GameDllTraceEntityAction action);
const char *GameDllTraceCallActionName(GameDllTraceCallAction action);
const char *GameDllTraceModelActionName(GameDllTraceModelAction action);
const char *GameDllTraceTextureActionName(GameDllTraceTextureAction action);
const char *GameDllVisibilityCheckActionName(
	GameDllVisibilityCheckAction action);
const char *GameDllClientPvsActionName(GameDllClientPvsAction action);

}
}
}

#endif
