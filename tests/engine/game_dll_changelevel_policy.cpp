#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll/game_dll_changelevel_policy.hpp"

using namespace xash::engine::server;

namespace
{

static bool TestChangeLevelRequestAdmission()
{
	const GameDllChangeLevelRequestPlan empty =
		BuildGameDllChangeLevelRequestPlan("", "lm", true, 2, 1, true);
	const GameDllChangeLevelRequestPlan inactive =
		BuildGameDllChangeLevelRequestPlan("c1a0", "lm", false, 2, 1, true);
	const GameDllChangeLevelRequestPlan duplicate =
		BuildGameDllChangeLevelRequestPlan("c1a0", "lm", true, 2, 2, true);
	const GameDllChangeLevelRequestPlan queued =
		BuildGameDllChangeLevelRequestPlan("c1a0", "lm", true, 3, 2, true);

	return empty.action ==
			GameDllChangeLevelRequestAction::IgnoreEmptyOrInactive &&
		inactive.action ==
			GameDllChangeLevelRequestAction::IgnoreEmptyOrInactive &&
		duplicate.action ==
			GameDllChangeLevelRequestAction::IgnoreDuplicateSpawncount &&
		duplicate.nextLastSpawncount == 2 &&
		queued.action == GameDllChangeLevelRequestAction::QueueChangeLevel &&
		queued.nextLastSpawncount == 3 &&
		queued.level == "c1a0" &&
		queued.landmark == "lm";
}

static bool TestLandmarkTruncation()
{
	const GameDllChangeLevelRequestPlan truncated =
		BuildGameDllChangeLevelRequestPlan(
			"c1a0", "landmark with spaces", true, 4, 3, true);
	const GameDllChangeLevelRequestPlan preserved =
		BuildGameDllChangeLevelRequestPlan(
			"c1a0", "landmark with spaces", true, 4, 3, false);

	return truncated.landmark == "landmark" &&
		preserved.landmark == "landmark with spaces";
}

static bool TestQueuedChangeLevelValidation()
{
	const GameDllQueuedChangeLevelPlan invalid =
		BuildGameDllQueuedChangeLevelPlan(
			"c1a0.bsp",
			"",
			"c0a0",
			kGameDllMapExists | kGameDllMapInvalidVersion,
			true,
			1,
			20);
	const GameDllQueuedChangeLevelPlan missing =
		BuildGameDllQueuedChangeLevelPlan(
			"missing",
			"",
			"c0a0",
			0,
			true,
			1,
			20);

	return invalid.action ==
			GameDllQueuedChangeLevelAction::RejectInvalidVersion &&
		invalid.mapName == "c1a0" &&
		missing.action == GameDllQueuedChangeLevelAction::RejectMissingMap;
}

static bool TestSmoothFallbacks()
{
	const GameDllQueuedChangeLevelPlan noLandmarkValidated =
		BuildGameDllQueuedChangeLevelPlan(
			"c1a1",
			"lm",
			"c1a0",
			kGameDllMapExists,
			true,
			1,
			20);
	const GameDllQueuedChangeLevelPlan noLandmarkUnvalidated =
		BuildGameDllQueuedChangeLevelPlan(
			"c1a1",
			"lm",
			"c1a0",
			kGameDllMapExists,
			false,
			1,
			20);
	const GameDllQueuedChangeLevelPlan multiplayer =
		BuildGameDllQueuedChangeLevelPlan(
			"c1a1",
			"lm",
			"c1a0",
			kGameDllMapExists | kGameDllMapHasLandmark,
			true,
			2,
			20);

	return noLandmarkValidated.action ==
			GameDllQueuedChangeLevelAction::QueueClassic &&
		noLandmarkValidated.smoothRequested &&
		!noLandmarkValidated.smoothQueued &&
		noLandmarkValidated.landmarkWarning &&
		noLandmarkUnvalidated.action ==
			GameDllQueuedChangeLevelAction::QueueSmooth &&
		noLandmarkUnvalidated.smoothQueued &&
		multiplayer.action == GameDllQueuedChangeLevelAction::QueueClassic &&
		multiplayer.multiplayerForcedClassic;
}

static bool TestSmoothRejects()
{
	const GameDllQueuedChangeLevelPlan sameMap =
		BuildGameDllQueuedChangeLevelPlan(
			"c1a0",
			"lm",
			"c1a0",
			kGameDllMapExists | kGameDllMapHasLandmark,
			true,
			1,
			20);
	const GameDllQueuedChangeLevelPlan early =
		BuildGameDllQueuedChangeLevelPlan(
			"c1a1",
			"lm",
			"c1a0",
			kGameDllMapExists | kGameDllMapHasLandmark,
			true,
			1,
			14);
	const GameDllQueuedChangeLevelPlan earlyUnvalidated =
		BuildGameDllQueuedChangeLevelPlan(
			"c1a1",
			"lm",
			"c1a0",
			kGameDllMapExists | kGameDllMapHasLandmark,
			false,
			1,
			14);

	return sameMap.action == GameDllQueuedChangeLevelAction::RejectSameMap &&
		early.action ==
			GameDllQueuedChangeLevelAction::RejectEarlyFrameLoop &&
		earlyUnvalidated.action == GameDllQueuedChangeLevelAction::QueueSmooth;
}

static bool TestEntityPatchPlans()
{
	const GameDllEntityPatchWritePlan openFail =
		BuildGameDllEntityPatchWritePlan("c1a0", false, 2);
	const GameDllEntityPatchWritePlan writeEmpty =
		BuildGameDllEntityPatchWritePlan("c1a0", true, 0);
	const GameDllEntityPatchWritePlan writeRemoved =
		BuildGameDllEntityPatchWritePlan("c1a1", true, 3);
	const GameDllEntityPatchWritePlan negative =
		BuildGameDllEntityPatchWritePlan("c1a2", true, -5);

	return openFail.action ==
			GameDllEntityPatchWriteAction::RejectOpenFailure &&
		openFail.path == "save/c1a0.HL3" &&
		writeEmpty.action == GameDllEntityPatchWriteAction::WritePatch &&
		writeEmpty.removedCount == 0 &&
		writeRemoved.action == GameDllEntityPatchWriteAction::WritePatch &&
		writeRemoved.path == "save/c1a1.HL3" &&
		writeRemoved.removedCount == 3 &&
		negative.removedCount == 0;
}

static bool TestDisplayNames()
{
	return std::strcmp(
			GameDllChangeLevelRequestActionName(
				GameDllChangeLevelRequestAction::QueueChangeLevel),
			"queue-changelevel") == 0 &&
		std::strcmp(
			GameDllQueuedChangeLevelActionName(
				GameDllQueuedChangeLevelAction::RejectEarlyFrameLoop),
			"reject-early-frame-loop") == 0 &&
		std::strcmp(
			GameDllEntityPatchWriteActionName(
				GameDllEntityPatchWriteAction::WritePatch),
			"write-patch") == 0;
}

}

int main()
{
	if (!TestChangeLevelRequestAdmission() ||
		!TestLandmarkTruncation() ||
		!TestQueuedChangeLevelValidation() ||
		!TestSmoothFallbacks() ||
		!TestSmoothRejects() ||
		!TestEntityPatchPlans() ||
		!TestDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
