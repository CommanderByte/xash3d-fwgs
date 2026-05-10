#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll_visibility_trace_policy.hpp"

using namespace xash::engine::server;

namespace
{

static bool TestTraceEntityFallbacks()
{
	return BuildGameDllTraceEntityAction(false) ==
			GameDllTraceEntityAction::UseWorldEntity &&
		BuildGameDllTraceEntityAction(true) ==
			GameDllTraceEntityAction::UseTraceEntity &&
		BuildGameDllTraceTossAction(false) ==
			GameDllTraceCallAction::SkipInvalidEntity &&
		BuildGameDllTraceTossAction(true) ==
			GameDllTraceCallAction::RunTrace &&
		BuildGameDllTraceTextureAction(false) ==
			GameDllTraceTextureAction::ReturnNull &&
		BuildGameDllTraceTextureAction(true) ==
			GameDllTraceTextureAction::TraceTexture;
}

static bool TestHullClamping()
{
	const GameDllTraceHullPlan low = BuildGameDllTraceHullPlan(-1);
	const GameDllTraceHullPlan valid = BuildGameDllTraceHullPlan(2);
	const GameDllTraceHullPlan high = BuildGameDllTraceHullPlan(4);

	return low.hullNumber == 0 &&
		low.clamped &&
		valid.hullNumber == 2 &&
		!valid.clamped &&
		high.hullNumber == 0 &&
		high.clamped;
}

static bool TestMonsterHullPlans()
{
	const GameDllTraceMonsterHullPlan invalid =
		BuildGameDllTraceMonsterHullPlan(false, true);
	const GameDllTraceMonsterHullPlan normal =
		BuildGameDllTraceMonsterHullPlan(true, false);
	const GameDllTraceMonsterHullPlan monsterClip =
		BuildGameDllTraceMonsterHullPlan(true, true);

	return invalid.action == GameDllTraceCallAction::SkipInvalidEntity &&
		!invalid.monsterClip &&
		normal.action == GameDllTraceCallAction::RunTrace &&
		!normal.monsterClip &&
		monsterClip.action == GameDllTraceCallAction::RunTrace &&
		monsterClip.monsterClip &&
		!BuildGameDllTraceMonsterHullResult(false, 1.0f) &&
		BuildGameDllTraceMonsterHullResult(true, 1.0f) &&
		BuildGameDllTraceMonsterHullResult(false, 0.5f);
}

static bool TestTraceModelPlans()
{
	const GameDllTraceModelPlan invalid =
		BuildGameDllTraceModelPlan(false, 1, true, true);
	const GameDllTraceModelPlan custom =
		BuildGameDllTraceModelPlan(true, -4, true, true);
	const GameDllTraceModelPlan brush =
		BuildGameDllTraceModelPlan(true, 2, false, true);
	const GameDllTraceModelPlan generic =
		BuildGameDllTraceModelPlan(true, 3, false, false);

	return invalid.action == GameDllTraceModelAction::SkipInvalidEntity &&
		custom.action == GameDllTraceModelAction::RunCustomClip &&
		custom.hullNumber == 0 &&
		custom.clampedHull &&
		brush.action == GameDllTraceModelAction::RunBrushWithTemporaryBsp &&
		brush.hullNumber == 2 &&
		generic.action == GameDllTraceModelAction::RunEntityClip &&
		generic.hullNumber == 3;
}

static bool TestFatVisibilityPlans()
{
	const GameDllFatVisibilityPlan normal =
		BuildGameDllFatVisibilityPlan(true, false, true, false, false, false);
	const GameDllFatVisibilityPlan noVisData =
		BuildGameDllFatVisibilityPlan(false, false, true, false, false, false);
	const GameDllFatVisibilityPlan noOrigin =
		BuildGameDllFatVisibilityPlan(true, false, false, false, true, true);
	const GameDllFatVisibilityPlan disabled =
		BuildGameDllFatVisibilityPlan(true, false, true, true, true, false);

	return !normal.fullVisibility &&
		!normal.mergeVisibility &&
		noVisData.fullVisibility &&
		noOrigin.fullVisibility &&
		noOrigin.mergeVisibility &&
		noOrigin.passiveAudioSet &&
		disabled.fullVisibility &&
		disabled.mergeVisibility;
}

static bool TestVisibilityCheckPlans()
{
	const GameDllVisibilityCheckPlan invalid =
		BuildGameDllVisibilityCheckPlan(
			false, true, false, false, false, -1, false);
	const GameDllVisibilityCheckPlan full =
		BuildGameDllVisibilityCheckPlan(
			true, false, false, false, false, -1, false);
	const GameDllVisibilityCheckPlan owner =
		BuildGameDllVisibilityCheckPlan(
			true, true, true, true, true, -1, false);
	const GameDllVisibilityCheckPlan leaf16 =
		BuildGameDllVisibilityCheckPlan(
			true, true, false, false, false, -1, false);
	const GameDllVisibilityCheckPlan headnode32 =
		BuildGameDllVisibilityCheckPlan(
			true, true, false, false, false, 12, true);

	return invalid.action == GameDllVisibilityCheckAction::ReturnInvisible &&
		full.action == GameDllVisibilityCheckAction::ReturnVisibleFull &&
		owner.action == GameDllVisibilityCheckAction::UpcastOwnerAndCheck &&
		leaf16.action == GameDllVisibilityCheckAction::CheckLeafList &&
		leaf16.leafCapacity == 48 &&
		!leaf16.largeLeafs &&
		headnode32.action ==
			GameDllVisibilityCheckAction::CheckLeafListThenHeadnode &&
		headnode32.leafCapacity == 24 &&
		headnode32.largeLeafs;
}

static bool TestVisibilityResults()
{
	const GameDllVisibilityHeadnodeResult invisible =
		BuildGameDllVisibilityHeadnodeResult(false, 5, false);
	const GameDllVisibilityHeadnodeResult visible16 =
		BuildGameDllVisibilityHeadnodeResult(true, 47, false);
	const GameDllVisibilityHeadnodeResult visible32 =
		BuildGameDllVisibilityHeadnodeResult(true, 23, true);

	return BuildGameDllVisibilityLeafResult(false) == 0 &&
		BuildGameDllVisibilityLeafResult(true) == 1 &&
		invisible.visibilityCode == 0 &&
		!invisible.shouldCacheLeaf &&
		visible16.visibilityCode == 2 &&
		visible16.shouldCacheLeaf &&
		visible16.nextLeafCount == 0 &&
		visible32.visibilityCode == 2 &&
		visible32.nextLeafCount == 0;
}

static bool TestClientPvsAndSkipPlayerPlans()
{
	const GameDllClientPvsPlan invalid =
		BuildGameDllClientPvsPlan(false, true, 0.2f);
	const GameDllClientPvsPlan cached =
		BuildGameDllClientPvsPlan(true, false, 0.05f);
	const GameDllClientPvsPlan refresh =
		BuildGameDllClientPvsPlan(true, true, 0.1f);
	const GameDllClientPvsPlan negative =
		BuildGameDllClientPvsPlan(true, false, -0.1f);

	return invalid.action == GameDllClientPvsAction::ReturnWorldEntity &&
		!invalid.mergePortalVisibility &&
		cached.action == GameDllClientPvsAction::UseCachedClientPvs &&
		!cached.mergePortalVisibility &&
		refresh.action == GameDllClientPvsAction::RefreshClientPvs &&
		refresh.mergePortalVisibility &&
		negative.action == GameDllClientPvsAction::RefreshClientPvs &&
		BuildGameDllClientPvsResult(false, true) ==
			GameDllTraceEntityAction::UseWorldEntity &&
		BuildGameDllClientPvsResult(true, false) ==
			GameDllTraceEntityAction::UseWorldEntity &&
		BuildGameDllClientPvsResult(true, true) ==
			GameDllTraceEntityAction::UseTraceEntity &&
		!BuildGameDllCanSkipPlayerResult(false, true) &&
		!BuildGameDllCanSkipPlayerResult(true, false) &&
		BuildGameDllCanSkipPlayerResult(true, true);
}

static bool TestDisplayNames()
{
	return std::strcmp(
			GameDllTraceEntityActionName(
				GameDllTraceEntityAction::UseWorldEntity),
			"use-world-entity") == 0 &&
		std::strcmp(
			GameDllTraceCallActionName(
				GameDllTraceCallAction::SkipInvalidEntity),
			"skip-invalid-entity") == 0 &&
		std::strcmp(
			GameDllTraceModelActionName(
				GameDllTraceModelAction::RunBrushWithTemporaryBsp),
			"run-brush-with-temporary-bsp") == 0 &&
		std::strcmp(
			GameDllTraceTextureActionName(
				GameDllTraceTextureAction::TraceTexture),
			"trace-texture") == 0 &&
		std::strcmp(
			GameDllVisibilityCheckActionName(
				GameDllVisibilityCheckAction::UpcastOwnerAndCheck),
			"upcast-owner-and-check") == 0 &&
		std::strcmp(
			GameDllClientPvsActionName(
				GameDllClientPvsAction::RefreshClientPvs),
			"refresh-client-pvs") == 0;
}

}

int main()
{
	if (!TestTraceEntityFallbacks() ||
		!TestHullClamping() ||
		!TestMonsterHullPlans() ||
		!TestTraceModelPlans() ||
		!TestFatVisibilityPlans() ||
		!TestVisibilityCheckPlans() ||
		!TestVisibilityResults() ||
		!TestClientPvsAndSkipPlayerPlans() ||
		!TestDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
