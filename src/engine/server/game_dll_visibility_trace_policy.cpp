#include "engine/server/game_dll_visibility_trace_policy.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

constexpr int kMinHullNumber = 0;
constexpr int kMaxHullNumber = 3;
constexpr int kMaxEntLeafs16 = 48;
constexpr int kMaxEntLeafs32 = 24;

GameDllTraceHullPlan SanitizeHull(int hullNumber)
{
	GameDllTraceHullPlan plan = {};
	plan.hullNumber = hullNumber;

	if (hullNumber < kMinHullNumber || hullNumber > kMaxHullNumber)
	{
		plan.hullNumber = 0;
		plan.clamped = true;
	}

	return plan;
}

int LeafCapacity(bool largeLeafs)
{
	return largeLeafs ? kMaxEntLeafs32 : kMaxEntLeafs16;
}

}

GameDllTraceEntityAction BuildGameDllTraceEntityAction(bool validTraceEntity)
{
	return validTraceEntity ?
		GameDllTraceEntityAction::UseTraceEntity :
		GameDllTraceEntityAction::UseWorldEntity;
}

GameDllTraceCallAction BuildGameDllTraceTossAction(bool validEntity)
{
	return validEntity ?
		GameDllTraceCallAction::RunTrace :
		GameDllTraceCallAction::SkipInvalidEntity;
}

GameDllTraceHullPlan BuildGameDllTraceHullPlan(int hullNumber)
{
	return SanitizeHull(hullNumber);
}

GameDllTraceMonsterHullPlan BuildGameDllTraceMonsterHullPlan(
	bool validEntity,
	bool monsterClipFlag)
{
	GameDllTraceMonsterHullPlan plan = {};
	plan.action = validEntity ?
		GameDllTraceCallAction::RunTrace :
		GameDllTraceCallAction::SkipInvalidEntity;
	plan.monsterClip = validEntity && monsterClipFlag;
	return plan;
}

bool BuildGameDllTraceMonsterHullResult(bool allSolid, float fraction)
{
	return allSolid || fraction != 1.0f;
}

GameDllTraceModelPlan BuildGameDllTraceModelPlan(
	bool validEntity,
	int hullNumber,
	bool solidCustom,
	bool modelIsBrush)
{
	GameDllTraceModelPlan plan = {};
	const GameDllTraceHullPlan hull = SanitizeHull(hullNumber);
	plan.hullNumber = hull.hullNumber;
	plan.clampedHull = hull.clamped;

	if (!validEntity)
	{
		plan.action = GameDllTraceModelAction::SkipInvalidEntity;
		return plan;
	}

	if (solidCustom)
	{
		plan.action = GameDllTraceModelAction::RunCustomClip;
		return plan;
	}

	if (modelIsBrush)
	{
		plan.action = GameDllTraceModelAction::RunBrushWithTemporaryBsp;
		return plan;
	}

	plan.action = GameDllTraceModelAction::RunEntityClip;
	return plan;
}

GameDllTraceTextureAction BuildGameDllTraceTextureAction(bool validEntity)
{
	return validEntity ?
		GameDllTraceTextureAction::TraceTexture :
		GameDllTraceTextureAction::ReturnNull;
}

GameDllFatVisibilityPlan BuildGameDllFatVisibilityPlan(
	bool hasVisData,
	bool svNoVis,
	bool originPresent,
	bool clientVisibilityDisabled,
	bool hostMergeVisibility,
	bool passiveAudioSet)
{
	GameDllFatVisibilityPlan plan = {};
	plan.fullVisibility =
		!hasVisData || svNoVis || !originPresent || clientVisibilityDisabled;
	plan.mergeVisibility = hostMergeVisibility;
	plan.passiveAudioSet = passiveAudioSet;
	return plan;
}

GameDllVisibilityCheckPlan BuildGameDllVisibilityCheckPlan(
	bool validEntity,
	bool hasVisibilitySet,
	bool customEntity,
	bool hasOwner,
	bool ownerIsClient,
	int headnode,
	bool largeLeafs)
{
	GameDllVisibilityCheckPlan plan = {};
	plan.largeLeafs = largeLeafs;
	plan.leafCapacity = LeafCapacity(largeLeafs);

	if (!validEntity)
	{
		plan.action = GameDllVisibilityCheckAction::ReturnInvisible;
		return plan;
	}

	if (!hasVisibilitySet)
	{
		plan.action = GameDllVisibilityCheckAction::ReturnVisibleFull;
		return plan;
	}

	if (customEntity && hasOwner && ownerIsClient)
	{
		plan.action = GameDllVisibilityCheckAction::UpcastOwnerAndCheck;
		return plan;
	}

	plan.action = headnode < 0 ?
		GameDllVisibilityCheckAction::CheckLeafList :
		GameDllVisibilityCheckAction::CheckLeafListThenHeadnode;
	return plan;
}

int BuildGameDllVisibilityLeafResult(bool anyLeafVisible)
{
	return anyLeafVisible ? 1 : 0;
}

GameDllVisibilityHeadnodeResult BuildGameDllVisibilityHeadnodeResult(
	bool headnodeVisible,
	int currentLeafCount,
	bool largeLeafs)
{
	GameDllVisibilityHeadnodeResult result = {};

	if (!headnodeVisible)
		return result;

	const int capacity = LeafCapacity(largeLeafs);
	result.visibilityCode = 2;
	result.shouldCacheLeaf = true;
	result.nextLeafCount = (currentLeafCount + 1) % capacity;
	return result;
}

GameDllClientPvsPlan BuildGameDllClientPvsPlan(
	bool validViewer,
	bool viewerIsMonster,
	float secondsSinceLastCheck)
{
	GameDllClientPvsPlan plan = {};

	if (!validViewer)
	{
		plan.action = GameDllClientPvsAction::ReturnWorldEntity;
		return plan;
	}

	plan.mergePortalVisibility = viewerIsMonster;
	plan.action = (secondsSinceLastCheck < 0.0f || secondsSinceLastCheck >= 0.1f) ?
		GameDllClientPvsAction::RefreshClientPvs :
		GameDllClientPvsAction::UseCachedClientPvs;
	return plan;
}

GameDllTraceEntityAction BuildGameDllClientPvsResult(
	bool hasSpawnedClient,
	bool visible)
{
	return (hasSpawnedClient && visible) ?
		GameDllTraceEntityAction::UseTraceEntity :
		GameDllTraceEntityAction::UseWorldEntity;
}

bool BuildGameDllCanSkipPlayerResult(bool hasClient, bool localWeapons)
{
	return hasClient && localWeapons;
}

const char *GameDllTraceEntityActionName(GameDllTraceEntityAction action)
{
	switch (action)
	{
	case GameDllTraceEntityAction::UseWorldEntity:
		return "use-world-entity";
	case GameDllTraceEntityAction::UseTraceEntity:
		return "use-trace-entity";
	}

	return "unknown";
}

const char *GameDllTraceCallActionName(GameDllTraceCallAction action)
{
	switch (action)
	{
	case GameDllTraceCallAction::SkipInvalidEntity:
		return "skip-invalid-entity";
	case GameDllTraceCallAction::RunTrace:
		return "run-trace";
	}

	return "unknown";
}

const char *GameDllTraceModelActionName(GameDllTraceModelAction action)
{
	switch (action)
	{
	case GameDllTraceModelAction::SkipInvalidEntity:
		return "skip-invalid-entity";
	case GameDllTraceModelAction::RunCustomClip:
		return "run-custom-clip";
	case GameDllTraceModelAction::RunBrushWithTemporaryBsp:
		return "run-brush-with-temporary-bsp";
	case GameDllTraceModelAction::RunEntityClip:
		return "run-entity-clip";
	}

	return "unknown";
}

const char *GameDllTraceTextureActionName(GameDllTraceTextureAction action)
{
	switch (action)
	{
	case GameDllTraceTextureAction::ReturnNull:
		return "return-null";
	case GameDllTraceTextureAction::TraceTexture:
		return "trace-texture";
	}

	return "unknown";
}

const char *GameDllVisibilityCheckActionName(
	GameDllVisibilityCheckAction action)
{
	switch (action)
	{
	case GameDllVisibilityCheckAction::ReturnInvisible:
		return "return-invisible";
	case GameDllVisibilityCheckAction::ReturnVisibleFull:
		return "return-visible-full";
	case GameDllVisibilityCheckAction::CheckLeafList:
		return "check-leaf-list";
	case GameDllVisibilityCheckAction::UpcastOwnerAndCheck:
		return "upcast-owner-and-check";
	case GameDllVisibilityCheckAction::CheckLeafListThenHeadnode:
		return "check-leaf-list-then-headnode";
	}

	return "unknown";
}

const char *GameDllClientPvsActionName(GameDllClientPvsAction action)
{
	switch (action)
	{
	case GameDllClientPvsAction::ReturnWorldEntity:
		return "return-world-entity";
	case GameDllClientPvsAction::UseCachedClientPvs:
		return "use-cached-client-pvs";
	case GameDllClientPvsAction::RefreshClientPvs:
		return "refresh-client-pvs";
	}

	return "unknown";
}

}
}
}
