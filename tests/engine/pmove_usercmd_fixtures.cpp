#include <cmath>
#include <cstdlib>

#include "pmove_usercmd_fixture_common.hpp"

using namespace xash::test::engine;

namespace
{

static bool NearlyEqual(double left, double right, double tolerance = 0.00001)
{
	return std::fabs(left - right) <= tolerance;
}

static PmoveFixturePlayerState PlayerState()
{
	PmoveFixturePlayerState player = {};
	player.entityIndex = 2;
	player.maxClients = 4;
	player.timebase = 1.5f;
	player.movevarsMaxspeed = 320.0f;
	player.health = 100.0f;
	player.movetype = 3;
	player.oldbuttons = 7;
	player.waterlevel = 1;
	player.watertype = -3;
	player.clientmaxspeed = 270.0f;
	player.origin = FixturePmoveVec3(1.0f, 2.0f, 3.0f);
	player.viewangles = FixturePmoveVec3(30.0f, 90.0f, 5.0f);
	player.velocity = FixturePmoveVec3(4.0f, 5.0f, 6.0f);
	player.basevelocity = FixturePmoveVec3(7.0f, 8.0f, 9.0f);
	player.viewOffset = FixturePmoveVec3(0.0f, 0.0f, 28.0f);
	player.movedir = FixturePmoveVec3(1.0f, 0.0f, 0.0f);
	player.punchangle = FixturePmoveVec3(2.0f, 3.0f, 4.0f);
	return player;
}

static bool TestFrozenCommandKeepsViewAndOptionallyImpulse()
{
	PmoveFixtureUsercmd command = FixturePmoveUsercmd(16, 3, 9);
	command.forwardmove = 100.0f;
	command.sidemove = -8.0f;
	command.upmove = 2.0f;
	command.viewangles = FixturePmoveVec3(10.0f, 20.0f, 30.0f);

	const PmoveFixtureUsercmd paused = BuildFrozenPmoveCommand(command, false);
	const PmoveFixtureUsercmd frozen = BuildFrozenPmoveCommand(command, true);

	return paused.msec == 0 &&
		paused.forwardmove == 0.0f &&
		paused.sidemove == 0.0f &&
		paused.upmove == 0.0f &&
		paused.buttons == 0 &&
		paused.impulse == 9 &&
		FixturePmoveVec3Equal(paused.viewangles, command.viewangles) &&
		frozen.impulse == 0;
}

static bool TestDroppedCommandRunPlanMatchesLegacyOrder()
{
	const PmoveFixtureRunPlan plan =
		BuildPmoveFixtureRunPlan(3, 2, 2, 100);

	return plan.count == 5 &&
		plan.steps[0].useLastCommand &&
		plan.steps[0].randomSeed == 0 &&
		!plan.steps[1].useLastCommand &&
		plan.steps[1].commandIndex == 3 &&
		plan.steps[1].randomSeed == 97 &&
		!plan.steps[2].useLastCommand &&
		plan.steps[2].commandIndex == 2 &&
		plan.steps[2].randomSeed == 98 &&
		!plan.steps[3].useLastCommand &&
		plan.steps[3].commandIndex == 1 &&
		plan.steps[3].randomSeed == 99 &&
		!plan.steps[4].useLastCommand &&
		plan.steps[4].commandIndex == 0 &&
		plan.steps[4].randomSeed == 100;
}

static bool TestLargeDropSkipsRecoveryButRunsCurrentCommands()
{
	const PmoveFixtureRunPlan plan =
		BuildPmoveFixtureRunPlan(24, 2, 2, 100);

	return plan.count == 2 &&
		plan.steps[0].commandIndex == 1 &&
		plan.steps[1].commandIndex == 0;
}

static bool TestTimebaseFixtureMatchesLegacyCommandAccounting()
{
	PmoveFixtureUsercmd commands[kPmoveFixtureMaxCommands] = {};
	commands[0] = FixturePmoveUsercmd(10, 0, 0);
	commands[1] = FixturePmoveUsercmd(20, 0, 0);
	commands[2] = FixturePmoveUsercmd(30, 0, 0);
	commands[3] = FixturePmoveUsercmd(40, 0, 0);
	const PmoveFixtureUsercmd lastCommand = FixturePmoveUsercmd(50, 0, 0);

	const double seconds = BuildPmoveFixtureRunCommandSeconds(
		lastCommand,
		commands,
		3,
		2,
		2);
	const double timebase = BuildPmoveFixtureTimeBase(
		10.0,
		0.05,
		lastCommand,
		commands,
		3,
		2,
		2);

	return NearlyEqual(seconds, 0.15) &&
		NearlyEqual(timebase, 9.9);
}

static bool TestSetupSnapshotModelsSafeComparableFields()
{
	PmoveFixturePlayerState player = PlayerState();
	player.ducking = true;
	const PmoveFixtureUsercmd command = FixturePmoveUsercmd(16, 5, 1);
	const PmoveFixtureSetupSnapshot snapshot =
		BuildPmoveFixtureSetupSnapshot(player, command);

	return snapshot.playerIndex == 1 &&
		snapshot.multiplayer &&
		snapshot.useHull == 1 &&
		snapshot.timeMilliseconds == 1500.0f &&
		FixturePmoveVec3Equal(snapshot.origin, player.origin) &&
		FixturePmoveVec3Equal(snapshot.angles, player.viewangles) &&
		snapshot.cmd.buttons == 5 &&
		snapshot.runfuncs &&
		snapshot.numPhysent == 0 &&
		snapshot.numVisent == 0 &&
		snapshot.numMoveent == 0;
}

static bool TestFinishSnapshotModelsReturnFieldsAndAngleRule()
{
	PmoveFixtureMoveResult move = {};
	move.origin = FixturePmoveVec3(3.0f, 4.0f, 5.0f);
	move.angles = FixturePmoveVec3(30.0f, 90.0f, 6.0f);
	move.velocity = FixturePmoveVec3(8.0f, 9.0f, 10.0f);
	move.cmd = FixturePmoveUsercmd(16, 11, 0);
	move.onground = 1;
	move.numPhysent = 2;
	move.runfuncs = true;

	const PmoveFixtureFinishSnapshot normal =
		BuildPmoveFixtureFinishSnapshot(move, false);
	const PmoveFixtureFinishSnapshot fixed =
		BuildPmoveFixtureFinishSnapshot(move, true);

	return FixturePmoveVec3Equal(normal.origin, move.origin) &&
		FixturePmoveVec3Equal(normal.viewangles, move.angles) &&
		normal.angles.x == -10.0f &&
		normal.angles.y == 90.0f &&
		normal.angles.z == 6.0f &&
		normal.oldbuttons == 11 &&
		normal.onGround &&
		!normal.runfuncs &&
		FixturePmoveVec3Equal(fixed.viewangles, FixturePmoveVec3(0.0f, 0.0f, 0.0f));
}

static bool TestSetupFinishSnapshotsSupportComparisonPlan()
{
	const PmoveFixturePlayerState player = PlayerState();
	const PmoveFixtureUsercmd command = FixturePmoveUsercmd(16, 5, 1);
	const PmoveFixtureSetupSnapshot setup =
		BuildPmoveFixtureSetupSnapshot(player, command);
	PmoveFixtureMoveResult move = {};
	move.origin = setup.origin;
	move.angles = setup.angles;
	move.velocity = FixturePmoveVec3(10.0f, 20.0f, 30.0f);
	move.cmd = command;
	move.onground = -1;
	move.numPhysent = 0;

	const PmoveFixtureFinishSnapshot finish =
		BuildPmoveFixtureFinishSnapshot(move, false);

	return FixturePmoveVec3Equal(setup.origin, finish.origin) &&
		FixturePmoveVec3Equal(setup.angles, finish.viewangles) &&
		finish.oldbuttons == command.buttons &&
		!finish.onGround &&
		!finish.runfuncs;
}

static bool TestCallbackMockInventoryCoversPmoveAndTouchReplay()
{
	const unsigned int mask = BuildPmoveFixtureRequiredCallbackMockMask();

	return FixturePmoveCallbackMockMaskHas(mask, kPmoveFixtureCallbackPlayerTrace) &&
		FixturePmoveCallbackMockMaskHas(mask, kPmoveFixtureCallbackTraceLine) &&
		FixturePmoveCallbackMockMaskHas(mask, kPmoveFixtureCallbackTestPlayerPosition) &&
		FixturePmoveCallbackMockMaskHas(mask, kPmoveFixtureCallbackPointContents) &&
		FixturePmoveCallbackMockMaskHas(mask, kPmoveFixtureCallbackTraceTexture) &&
		FixturePmoveCallbackMockMaskHas(mask, kPmoveFixtureCallbackPlaybackEvent) &&
		FixturePmoveCallbackMockMaskHas(mask, kPmoveFixtureCallbackPlaySound) &&
		FixturePmoveCallbackMockMaskHas(mask, kPmoveFixtureCallbackModelQueries) &&
		FixturePmoveCallbackMockMaskHas(mask, kPmoveFixtureCallbackFileAccess) &&
		FixturePmoveCallbackMockMaskHas(mask, kPmoveFixtureCallbackTouchReplay);
}

static bool TestUnlagHistoryFixtureReusesModernTimingPolicy()
{
	PmoveFixtureUnlagHistory history = {};
	history.realtime = 100.0f;
	history.clientLatency = 0.1f;
	history.maxUnlag = 0.0f;
	history.lerpMsec = 50;
	history.nextMessageInterval = 0.01f;
	history.pushSeconds = 0.0f;

	return NearlyEqual(BuildPmoveFixtureUnlagTargetTime(history), 99.85);
}

}

int main()
{
	if (!TestFrozenCommandKeepsViewAndOptionallyImpulse() ||
		!TestDroppedCommandRunPlanMatchesLegacyOrder() ||
		!TestLargeDropSkipsRecoveryButRunsCurrentCommands() ||
		!TestTimebaseFixtureMatchesLegacyCommandAccounting() ||
		!TestSetupSnapshotModelsSafeComparableFields() ||
		!TestFinishSnapshotModelsReturnFieldsAndAngleRule() ||
		!TestSetupFinishSnapshotsSupportComparisonPlan() ||
		!TestCallbackMockInventoryCoversPmoveAndTouchReplay() ||
		!TestUnlagHistoryFixtureReusesModernTimingPolicy())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
