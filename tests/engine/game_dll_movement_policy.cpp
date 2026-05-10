#include <cmath>
#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll_movement_policy.hpp"

using namespace xash::engine::server;

namespace
{

static bool NearlyEqual(float a, float b, float tolerance = 0.01f)
{
	return std::fabs(a - b) <= tolerance;
}

static bool NearlyEqualDouble(double a, double b, double tolerance = 0.00001)
{
	return std::fabs(a - b) <= tolerance;
}

static bool TestMoveToOriginPlans()
{
	const GameDllMoveToOriginPlan missingGoal =
		BuildGameDllMoveToOriginPlan(false, false, 0, kGameDllMoveNormal);
	const GameDllMoveToOriginPlan invalid =
		BuildGameDllMoveToOriginPlan(true, false, 0, kGameDllMoveNormal);
	const GameDllMoveToOriginPlan immobile =
		BuildGameDllMoveToOriginPlan(true, true, 0, kGameDllMoveNormal);
	const GameDllMoveToOriginPlan normal =
		BuildGameDllMoveToOriginPlan(
			true, true, kGameDllMoveFlagOnGround, kGameDllMoveNormal);
	const GameDllMoveToOriginPlan groundStrafe =
		BuildGameDllMoveToOriginPlan(
			true, true, kGameDllMoveFlagOnGround, 1);
	const GameDllMoveToOriginPlan swimStrafe =
		BuildGameDllMoveToOriginPlan(
			true, true, kGameDllMoveFlagSwim, 1);

	return missingGoal.action == GameDllMoveToOriginAction::SkipMissingGoal &&
		invalid.action == GameDllMoveToOriginAction::SkipInvalidEntity &&
		immobile.action == GameDllMoveToOriginAction::SkipImmobileEntity &&
		normal.action == GameDllMoveToOriginAction::StepTowardIdealYaw &&
		!normal.usesVerticalGoal &&
		groundStrafe.action ==
			GameDllMoveToOriginAction::FlyDirectionTowardGoal &&
		!groundStrafe.usesVerticalGoal &&
		swimStrafe.action ==
			GameDllMoveToOriginAction::FlyDirectionTowardGoal &&
		swimStrafe.usesVerticalGoal;
}

static bool TestAngularMovePlans()
{
	const GameDllAngularMovePlan invalid =
		BuildGameDllAngularMovePlan(false, 45.0f, 10.0f, 5.0f);
	const GameDllAngularMovePlan same =
		BuildGameDllAngularMovePlan(true, 90.0f, 90.0f, 5.0f);
	const GameDllAngularMovePlan wrapsForward =
		BuildGameDllAngularMovePlan(true, 10.0f, 350.0f, 5.0f);
	const GameDllAngularMovePlan wrapsBackward =
		BuildGameDllAngularMovePlan(true, 270.0f, 10.0f, 20.0f);
	const GameDllAngularMovePlan noSpeed =
		BuildGameDllAngularMovePlan(true, 180.0f, 90.0f, 0.0f);

	return invalid.action == GameDllAngularMoveAction::SkipInvalidEntity &&
		same.action == GameDllAngularMoveAction::ApplyAngle &&
		NearlyEqual(same.nextAngle, 90.0f) &&
		NearlyEqual(wrapsForward.nextAngle, 355.0f) &&
		NearlyEqual(wrapsBackward.nextAngle, 350.0f) &&
		NearlyEqual(noSpeed.nextAngle, 90.0f);
}

static bool TestWalkMovePlans()
{
	const GameDllWalkMovePlan invalid =
		BuildGameDllWalkMovePlan(false, 0, 0.0f, 16.0f, 99);
	const GameDllWalkMovePlan immobile =
		BuildGameDllWalkMovePlan(true, 0, 0.0f, 16.0f, 99);
	const GameDllWalkMovePlan normal =
		BuildGameDllWalkMovePlan(
			true,
			kGameDllMoveFlagOnGround,
			0.0f,
			16.0f,
			kGameDllWalkMoveNormal);
	const GameDllWalkMovePlan worldOnly =
		BuildGameDllWalkMovePlan(
			true,
			kGameDllMoveFlagFly,
			90.0f,
			16.0f,
			kGameDllWalkMoveWorldOnly);
	const GameDllWalkMovePlan checkOnly =
		BuildGameDllWalkMovePlan(
			true,
			kGameDllMoveFlagSwim,
			180.0f,
			8.0f,
			kGameDllWalkMoveCheckOnly);
	const GameDllWalkMovePlan badMode =
		BuildGameDllWalkMovePlan(
			true,
			kGameDllMoveFlagOnGround,
			45.0f,
			8.0f,
			99);

	return invalid.action == GameDllWalkMoveAction::SkipInvalidEntity &&
		immobile.action == GameDllWalkMoveAction::SkipImmobileEntity &&
		normal.action == GameDllWalkMoveAction::MoveStepRelink &&
		NearlyEqual(normal.move.x, 16.0f) &&
		NearlyEqual(normal.move.y, 0.0f) &&
		NearlyEqual(normal.move.z, 0.0f) &&
		worldOnly.action == GameDllWalkMoveAction::MoveTestWorldOnly &&
		NearlyEqual(worldOnly.move.x, 0.0f) &&
		NearlyEqual(worldOnly.move.y, 16.0f) &&
		checkOnly.action == GameDllWalkMoveAction::MoveStepCheckOnly &&
		NearlyEqual(checkOnly.move.x, -8.0f) &&
		NearlyEqual(checkOnly.move.y, 0.0f) &&
		badMode.action == GameDllWalkMoveAction::FatalUnknownMode;
}

static bool TestSetOriginAndMaxspeedPlans()
{
	const GameDllClientMaxspeedPlan missing =
		BuildGameDllClientMaxspeedPlan(false, 999.0f, 320.0f);
	const GameDllClientMaxspeedPlan clampedHigh =
		BuildGameDllClientMaxspeedPlan(true, 999.0f, 320.0f);
	const GameDllClientMaxspeedPlan clampedLow =
		BuildGameDllClientMaxspeedPlan(true, -999.0f, 320.0f);
	const GameDllClientMaxspeedPlan rounded =
		BuildGameDllClientMaxspeedPlan(true, 123.4f, 320.0f);

	return BuildGameDllSetOriginAction(false) ==
			GameDllSetOriginAction::SkipInvalidEntity &&
		BuildGameDllSetOriginAction(true) ==
			GameDllSetOriginAction::CopyOriginAndRelink &&
		missing.action == GameDllClientMaxspeedAction::SkipMissingClient &&
		missing.physInfoValue.empty() &&
		clampedHigh.action == GameDllClientMaxspeedAction::SetMaxspeed &&
		NearlyEqual(clampedHigh.maxspeed, 320.0f) &&
		clampedHigh.physInfoValue == "320" &&
		NearlyEqual(clampedLow.maxspeed, -320.0f) &&
		clampedLow.physInfoValue == "-320" &&
		NearlyEqual(rounded.maxspeed, 123.4f) &&
		rounded.physInfoValue == "123";
}

static bool TestRunPlayerMovePlans()
{
	const GameDllVector3 viewAngles = { 1.0f, 2.0f, 3.0f };
	const GameDllRunPlayerMovePlan missing =
		BuildGameDllRunPlayerMovePlan(
			false, true, 10.0, 0.015, viewAngles, 100.0f, 20.0f, 5.0f,
			7, 9, 20);
	const GameDllRunPlayerMovePlan realClient =
		BuildGameDllRunPlayerMovePlan(
			true, false, 10.0, 0.015, viewAngles, 100.0f, 20.0f, 5.0f,
			7, 9, 20);
	const GameDllRunPlayerMovePlan fakeClient =
		BuildGameDllRunPlayerMovePlan(
			true, true, 10.0, 0.015, viewAngles, 100.0f, 20.0f, 5.0f,
			7, 9, 20);

	return missing.action ==
			GameDllRunPlayerMoveAction::SkipMissingSpawnedClient &&
		realClient.action == GameDllRunPlayerMoveAction::SkipRealClient &&
		fakeClient.action == GameDllRunPlayerMoveAction::RunFakeClientMove &&
		NearlyEqualDouble(fakeClient.timebase, 9.995) &&
		NearlyEqual(fakeClient.command.viewAngles.x, 1.0f) &&
		NearlyEqual(fakeClient.command.viewAngles.y, 2.0f) &&
		NearlyEqual(fakeClient.command.viewAngles.z, 3.0f) &&
		NearlyEqual(fakeClient.command.forwardMove, 100.0f) &&
		NearlyEqual(fakeClient.command.sideMove, 20.0f) &&
		NearlyEqual(fakeClient.command.upMove, 5.0f) &&
		fakeClient.command.buttons == 7 &&
		fakeClient.command.impulse == 9 &&
		fakeClient.command.msec == 20;
}

static bool TestFakeClientNameAndDisplayNames()
{
	return BuildGameDllFakeClientName(nullptr) == "Bot" &&
		BuildGameDllFakeClientName("") == "Bot" &&
		BuildGameDllFakeClientName("barney") == "barney" &&
		std::strcmp(
			GameDllMoveToOriginActionName(
				GameDllMoveToOriginAction::FlyDirectionTowardGoal),
			"fly-direction-toward-goal") == 0 &&
		std::strcmp(
			GameDllAngularMoveActionName(
				GameDllAngularMoveAction::ApplyAngle),
			"apply-angle") == 0 &&
		std::strcmp(
			GameDllWalkMoveActionName(
				GameDllWalkMoveAction::FatalUnknownMode),
			"fatal-unknown-mode") == 0 &&
		std::strcmp(
			GameDllSetOriginActionName(
				GameDllSetOriginAction::CopyOriginAndRelink),
			"copy-origin-and-relink") == 0 &&
		std::strcmp(
			GameDllClientMaxspeedActionName(
				GameDllClientMaxspeedAction::SetMaxspeed),
			"set-maxspeed") == 0 &&
		std::strcmp(
			GameDllRunPlayerMoveActionName(
				GameDllRunPlayerMoveAction::RunFakeClientMove),
			"run-fake-client-move") == 0;
}

}

int main()
{
	if (!TestMoveToOriginPlans() ||
		!TestAngularMovePlans() ||
		!TestWalkMovePlans() ||
		!TestSetOriginAndMaxspeedPlans() ||
		!TestRunPlayerMovePlans() ||
		!TestFakeClientNameAndDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
