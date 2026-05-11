#include <cmath>
#include <cstdlib>

#include "engine/server/world/server_movement_constraints.hpp"
#include "engine/server/world/server_physics_routing_policy.hpp"
#include "engine/server/world/server_pmove_bridge_policy.hpp"
#include "engine/server/world/server_world_link_policy.hpp"
#include "engine/server/world/server_world_trace_policy.hpp"

using namespace xash::engine::server;

namespace
{

namespace legacy_movement
{

constexpr int kServerMonsterMoveNormal = 0;
constexpr int kServerMonsterMoveStrafe = 1;
constexpr float kServerMoveEpsilon = 0.01f;
constexpr int kServerFlyMoveMaxClipPlanes = 5;
constexpr int kTraceMoveNormal = 0;
constexpr int kPmSharedMaxClipPlanes = 5;
constexpr float kTraceDistanceEpsilon = 1.0f / 32.0f;

}

namespace legacy_physics
{

constexpr int kMoveTypeNone = 0;
constexpr int kMoveTypeWalk = 3;
constexpr int kMoveTypeStep = 4;
constexpr int kMoveTypeFly = 5;
constexpr int kMoveTypeToss = 6;
constexpr int kMoveTypePush = 7;
constexpr int kMoveTypeNoclip = 8;
constexpr int kMoveTypeFlyMissile = 9;
constexpr int kMoveTypeBounce = 10;
constexpr int kMoveTypeBounceMissile = 11;
constexpr int kMoveTypeFollow = 12;
constexpr int kMoveTypePushStep = 13;
constexpr int kMoveTypeCompound = 14;

}

static bool NearlyEqual(float left, float right, float tolerance = 0.000001f)
{
	return std::fabs(left - right) <= tolerance;
}

static bool TestLegacyValueMirrors()
{
	return kServerMoveNormal == legacy_movement::kServerMonsterMoveNormal &&
		kServerMoveStrafe == legacy_movement::kServerMonsterMoveStrafe &&
		NearlyEqual(kServerMoveEpsilon, legacy_movement::kServerMoveEpsilon) &&
		kServerMaxClipPlanes == legacy_movement::kServerFlyMoveMaxClipPlanes;
}

static bool TestMonsterMoveTypeHelpers()
{
	return ToLegacyMonsterMoveType(ServerMonsterMoveType::Normal) ==
			legacy_movement::kServerMonsterMoveNormal &&
		ToLegacyMonsterMoveType(ServerMonsterMoveType::Strafe) ==
			legacy_movement::kServerMonsterMoveStrafe &&
		IsServerMonsterNormalMoveType(0) &&
		!IsServerMonsterNormalMoveType(1) &&
		IsServerMonsterStrafeMoveType(1) &&
		!IsServerMonsterStrafeMoveType(0) &&
		!IsServerMonsterNormalMoveType(2) &&
		!IsServerMonsterStrafeMoveType(2);
}

static bool TestFlyMoveClipPlanePolicy()
{
	return ServerFlyMoveClipIterationLimit() ==
			legacy_movement::kServerFlyMoveMaxClipPlanes - 1 &&
		!ServerFlyMoveCanAddClipPlane(-1) &&
		ServerFlyMoveCanAddClipPlane(0) &&
		ServerFlyMoveCanAddClipPlane(4) &&
		!ServerFlyMoveCanAddClipPlane(5);
}

static bool TestMovementSnapshot()
{
	const ServerMovementConstraintSnapshot snapshot =
		BuildServerMovementConstraintSnapshot();

	return snapshot.monsterMoveNormal ==
			legacy_movement::kServerMonsterMoveNormal &&
		snapshot.monsterMoveStrafe ==
			legacy_movement::kServerMonsterMoveStrafe &&
		NearlyEqual(
			snapshot.movementEpsilon,
			legacy_movement::kServerMoveEpsilon) &&
		snapshot.flyMoveClipPlaneLimit ==
			legacy_movement::kServerFlyMoveMaxClipPlanes &&
		snapshot.flyMoveClipIterationLimit ==
			legacy_movement::kServerFlyMoveMaxClipPlanes - 1;
}

static bool TestAdjacentConstantsAreNotMergedByAssumption()
{
	return kServerMoveNormal == legacy_movement::kTraceMoveNormal &&
		kServerMaxClipPlanes == legacy_movement::kPmSharedMaxClipPlanes &&
		!NearlyEqual(
			kServerMoveEpsilon,
			legacy_movement::kTraceDistanceEpsilon);
}

static bool TestPhysicsHandlerRouting()
{
	return SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypeNone) == ServerPhysicsHandler::None &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypeNoclip) == ServerPhysicsHandler::Noclip &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypeFollow) == ServerPhysicsHandler::Follow &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypeCompound) ==
			ServerPhysicsHandler::Compound &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypeStep) == ServerPhysicsHandler::Step &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypePushStep) == ServerPhysicsHandler::Step &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypeFly) == ServerPhysicsHandler::Toss &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypeToss) == ServerPhysicsHandler::Toss &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypeBounce) == ServerPhysicsHandler::Toss &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypeFlyMissile) ==
			ServerPhysicsHandler::Toss &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypeBounceMissile) ==
			ServerPhysicsHandler::Toss &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypePush) == ServerPhysicsHandler::Pusher &&
		SelectServerPhysicsHandlerForMoveType(
			legacy_physics::kMoveTypeWalk) ==
			ServerPhysicsHandler::InvalidWalk &&
		SelectServerPhysicsHandlerForMoveType(99) ==
			ServerPhysicsHandler::Unsupported;
}

static bool TestPusherCandidateFilter()
{
	return !PusherConsidersMoveType(legacy_physics::kMoveTypeNone) &&
		!PusherConsidersMoveType(legacy_physics::kMoveTypePush) &&
		!PusherConsidersMoveType(legacy_physics::kMoveTypeFollow) &&
		!PusherConsidersMoveType(legacy_physics::kMoveTypeNoclip) &&
		!PusherConsidersMoveType(legacy_physics::kMoveTypeCompound) &&
		PusherConsidersMoveType(legacy_physics::kMoveTypeWalk) &&
		PusherConsidersMoveType(legacy_physics::kMoveTypeStep) &&
		PusherConsidersMoveType(legacy_physics::kMoveTypeFly) &&
		PusherConsidersMoveType(legacy_physics::kMoveTypeToss) &&
		PusherConsidersMoveType(legacy_physics::kMoveTypeFlyMissile) &&
		PusherConsidersMoveType(legacy_physics::kMoveTypeBounce) &&
		PusherConsidersMoveType(legacy_physics::kMoveTypeBounceMissile) &&
		PusherConsidersMoveType(legacy_physics::kMoveTypePushStep) &&
		PusherConsidersMoveType(99);
}

static bool TestPreciseBlockingFilter()
{
	return PushedEntityUsesPreciseBlocking(legacy_physics::kMoveTypeWalk) &&
		PushedEntityUsesPreciseBlocking(legacy_physics::kMoveTypeStep) &&
		PushedEntityUsesPreciseBlocking(legacy_physics::kMoveTypePushStep) &&
		!PushedEntityUsesPreciseBlocking(legacy_physics::kMoveTypeNone) &&
		!PushedEntityUsesPreciseBlocking(legacy_physics::kMoveTypePush) &&
		!PushedEntityUsesPreciseBlocking(legacy_physics::kMoveTypeFly) &&
		!PushedEntityUsesPreciseBlocking(legacy_physics::kMoveTypeToss) &&
		!PushedEntityUsesPreciseBlocking(99);
}

static PmoveUnlagAdmissionFacts BuildAllowedUnlagFacts()
{
	PmoveUnlagAdmissionFacts facts = {};
	facts.maxClients = 2;
	facts.gameAllowsLagCompensation = true;
	facts.serverUnlagEnabled = true;
	facts.clientLagCompensationEnabled = true;
	facts.clientSpawned = true;
	return facts;
}

static bool TestUnlagAdmission()
{
	PmoveUnlagAdmissionFacts facts = BuildAllowedUnlagFacts();
	if (!ShouldEnablePmoveUnlag(facts))
		return false;

	facts = BuildAllowedUnlagFacts();
	facts.maxClients = 1;
	if (ShouldEnablePmoveUnlag(facts))
		return false;

	facts = BuildAllowedUnlagFacts();
	facts.gameAllowsLagCompensation = false;
	if (ShouldEnablePmoveUnlag(facts))
		return false;

	facts = BuildAllowedUnlagFacts();
	facts.serverUnlagEnabled = false;
	if (ShouldEnablePmoveUnlag(facts))
		return false;

	facts = BuildAllowedUnlagFacts();
	facts.clientLagCompensationEnabled = false;
	if (ShouldEnablePmoveUnlag(facts))
		return false;

	facts = BuildAllowedUnlagFacts();
	facts.clientSpawned = false;
	return !ShouldEnablePmoveUnlag(facts);
}

static bool TestPlayerEntityIndexAndInterpolantAdmission()
{
	return !IsPmovePlayerEntityIndex(0, 4) &&
		IsPmovePlayerEntityIndex(1, 4) &&
		IsPmovePlayerEntityIndex(4, 4) &&
		!IsPmovePlayerEntityIndex(5, 4) &&
		!ShouldUsePmoveInterpolatedPlayer(0, 4, true, true) &&
		!ShouldUsePmoveInterpolatedPlayer(1, 4, false, true) &&
		!ShouldUsePmoveInterpolatedPlayer(1, 4, true, false) &&
		ShouldUsePmoveInterpolatedPlayer(1, 4, true, true);
}

static bool TestTeleportThresholdIsStrict()
{
	const PmoveVector3 origin = { 0.0f, 0.0f, 0.0f };
	const PmoveVector3 exactlyThreshold = {
		kPmoveUnlagTeleportDistance,
		0.0f,
		0.0f,
	};
	const PmoveVector3 beyondThreshold = {
		0.0f,
		0.0f,
		-kPmoveUnlagTeleportDistance - 0.001f,
	};

	return !IsPmoveUnlagTeleport(origin, exactlyThreshold) &&
		IsPmoveUnlagTeleport(origin, beyondThreshold);
}

static bool TestLatencyPlan()
{
	const PmoveUnlagLatencyPlan noMaxUnlag =
		BuildPmoveUnlagLatencyPlan(2.0f, 0.0f);
	const PmoveUnlagLatencyPlan positiveMaxUnlag =
		BuildPmoveUnlagLatencyPlan(1.0f, 0.25f);
	const PmoveUnlagLatencyPlan negativeMaxUnlag =
		BuildPmoveUnlagLatencyPlan(1.0f, -1.0f);

	return NearlyEqual(noMaxUnlag.latency, kPmoveUnlagMaxLatencySeconds) &&
		NearlyEqual(noMaxUnlag.normalizedMaxUnlag, 0.0f) &&
		!noMaxUnlag.clampMaxUnlagCvarToZero &&
		NearlyEqual(positiveMaxUnlag.latency, 0.25f) &&
		NearlyEqual(positiveMaxUnlag.normalizedMaxUnlag, 0.25f) &&
		!positiveMaxUnlag.clampMaxUnlagCvarToZero &&
		NearlyEqual(negativeMaxUnlag.latency, 0.0f) &&
		NearlyEqual(negativeMaxUnlag.normalizedMaxUnlag, 0.0f) &&
		negativeMaxUnlag.clampMaxUnlagCvarToZero;
}

static bool TestLerpAndTargetTime()
{
	return NearlyEqual(
			BuildPmoveLerpSeconds(250, 0.0f),
			kPmoveUnlagMaxLerpSeconds) &&
		NearlyEqual(BuildPmoveLerpSeconds(10, 0.05f), 0.05f) &&
		NearlyEqual(BuildPmoveLerpSeconds(50, 0.01f), 0.05f) &&
		NearlyEqual(
			BuildPmoveUnlagTargetTime(100.0f, 0.1f, 0.05f, 0.0f),
			99.85f) &&
		NearlyEqual(
			BuildPmoveUnlagTargetTime(100.0f, 0.1f, 0.05f, 1.0f),
			100.0f);
}

static bool TestInterpolationFraction()
{
	return NearlyEqual(BuildPmoveInterpolationFraction(10.0f, 5.0f, 5.0f),
			0.0f) &&
		NearlyEqual(BuildPmoveInterpolationFraction(4.0f, 5.0f, 10.0f),
			0.0f) &&
		NearlyEqual(BuildPmoveInterpolationFraction(12.0f, 5.0f, 10.0f),
			1.0f) &&
		NearlyEqual(BuildPmoveInterpolationFraction(7.5f, 5.0f, 10.0f),
			0.5f);
}

static bool TestSplitAxisChoosesWiderDimension()
{
	return SelectWorldAreaSplitAxis(128.0f, 64.0f) == 0 &&
		SelectWorldAreaSplitAxis(64.0f, 128.0f) == 1 &&
		SelectWorldAreaSplitAxis(64.0f, 64.0f) == 1;
}

static bool TestSplitDistanceUsesMidpoint()
{
	return BuildWorldAreaSplitDistance(-32.0f, 96.0f) == 32.0f &&
		BuildWorldAreaSplitDistance(8.0f, 10.0f) == 9.0f;
}

static bool TestLinkChildSelectsStrictPositiveAndNegativeSides()
{
	return SelectWorldAreaLinkChild(10.1f, 12.0f, 10.0f) ==
			kWorldAreaChildPositive &&
		SelectWorldAreaLinkChild(2.0f, 9.9f, 10.0f) ==
			kWorldAreaChildNegative;
}

static bool TestLinkChildStopsWhenBoundsCrossOrTouchSplit()
{
	return SelectWorldAreaLinkChild(9.0f, 11.0f, 10.0f) ==
			kWorldAreaChildNone &&
		SelectWorldAreaLinkChild(10.0f, 12.0f, 10.0f) ==
			kWorldAreaChildNone &&
		SelectWorldAreaLinkChild(8.0f, 10.0f, 10.0f) ==
			kWorldAreaChildNone &&
		SelectWorldAreaLinkChild(10.0f, 10.0f, 10.0f) ==
			kWorldAreaChildNone;
}

static bool TestTraversalMaskUsesStrictPositiveAndNegativeTests()
{
	return BuildWorldAreaTraversalMask(11.0f, 12.0f, 10.0f) ==
			kWorldAreaChildPositiveMask &&
		BuildWorldAreaTraversalMask(8.0f, 9.0f, 10.0f) ==
			kWorldAreaChildNegativeMask &&
		BuildWorldAreaTraversalMask(8.0f, 12.0f, 10.0f) ==
			(kWorldAreaChildPositiveMask | kWorldAreaChildNegativeMask);
}

static bool TestTraversalMaskPreservesSplitPlaneEdgeBehavior()
{
	return BuildWorldAreaTraversalMask(10.0f, 12.0f, 10.0f) ==
			kWorldAreaChildPositiveMask &&
		BuildWorldAreaTraversalMask(8.0f, 10.0f, 10.0f) ==
			kWorldAreaChildNegativeMask &&
		BuildWorldAreaTraversalMask(10.0f, 10.0f, 10.0f) == 0;
}

static bool TestDecodesMoveTypeAndIgnoreTransparentBits()
{
	const ServerWorldMoveClipPlan plan =
		BuildServerWorldMoveClipPlan((3 << 8) | 2, false, false, 99);

	return plan.moveType == 2 &&
		plan.ignoreTransparent == 3 &&
		!plan.monsterClip &&
		!plan.useMissileBounds;
}

static bool TestQuakeCompatibleSuppressesRequestedMonsterClip()
{
	const ServerWorldMoveClipPlan normal =
		BuildServerWorldMoveClipPlan(1, true, false, 99);
	const ServerWorldMoveClipPlan quake =
		BuildServerWorldMoveClipPlan(1, true, true, 99);

	return normal.monsterClip && !quake.monsterClip;
}

static bool TestMissileMoveTypeSelectsMissileBounds()
{
	const ServerWorldMoveClipPlan plan =
		BuildServerWorldMoveClipPlan((5 << 8) | 3, false, false, 3);

	return plan.moveType == 3 &&
		plan.ignoreTransparent == 5 &&
		plan.useMissileBounds;
}

}

int main()
{
	if (!TestLegacyValueMirrors() ||
		!TestMonsterMoveTypeHelpers() ||
		!TestFlyMoveClipPlanePolicy() ||
		!TestMovementSnapshot() ||
		!TestAdjacentConstantsAreNotMergedByAssumption() ||
		!TestPhysicsHandlerRouting() ||
		!TestPusherCandidateFilter() ||
		!TestPreciseBlockingFilter() ||
		!TestUnlagAdmission() ||
		!TestPlayerEntityIndexAndInterpolantAdmission() ||
		!TestTeleportThresholdIsStrict() ||
		!TestLatencyPlan() ||
		!TestLerpAndTargetTime() ||
		!TestInterpolationFraction() ||
		!TestSplitAxisChoosesWiderDimension() ||
		!TestSplitDistanceUsesMidpoint() ||
		!TestLinkChildSelectsStrictPositiveAndNegativeSides() ||
		!TestLinkChildStopsWhenBoundsCrossOrTouchSplit() ||
		!TestTraversalMaskUsesStrictPositiveAndNegativeTests() ||
		!TestTraversalMaskPreservesSplitPlaneEdgeBehavior() ||
		!TestDecodesMoveTypeAndIgnoreTransparentBits() ||
		!TestQuakeCompatibleSuppressesRequestedMonsterClip() ||
		!TestMissileMoveTypeSelectsMissileBounds())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
