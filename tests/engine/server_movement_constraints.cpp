#include <cmath>
#include <cstdlib>

#include "engine/server/server_movement_constraints.hpp"

using namespace xash::engine::server;

namespace
{

namespace legacy
{

constexpr int kServerMonsterMoveNormal = 0;
constexpr int kServerMonsterMoveStrafe = 1;
constexpr float kServerMoveEpsilon = 0.01f;
constexpr int kServerFlyMoveMaxClipPlanes = 5;
constexpr int kTraceMoveNormal = 0;
constexpr int kPmSharedMaxClipPlanes = 5;
constexpr float kTraceDistanceEpsilon = 1.0f / 32.0f;

}

static bool NearlyEqual(float left, float right)
{
	return std::fabs(left - right) <= 0.000001f;
}

static bool TestLegacyValueMirrors()
{
	return kServerMoveNormal == legacy::kServerMonsterMoveNormal &&
		kServerMoveStrafe == legacy::kServerMonsterMoveStrafe &&
		NearlyEqual(kServerMoveEpsilon, legacy::kServerMoveEpsilon) &&
		kServerMaxClipPlanes == legacy::kServerFlyMoveMaxClipPlanes;
}

static bool TestMonsterMoveTypeHelpers()
{
	return ToLegacyMonsterMoveType(ServerMonsterMoveType::Normal) ==
			legacy::kServerMonsterMoveNormal &&
		ToLegacyMonsterMoveType(ServerMonsterMoveType::Strafe) ==
			legacy::kServerMonsterMoveStrafe &&
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
			legacy::kServerFlyMoveMaxClipPlanes - 1 &&
		!ServerFlyMoveCanAddClipPlane(-1) &&
		ServerFlyMoveCanAddClipPlane(0) &&
		ServerFlyMoveCanAddClipPlane(4) &&
		!ServerFlyMoveCanAddClipPlane(5);
}

static bool TestSnapshot()
{
	const ServerMovementConstraintSnapshot snapshot =
		BuildServerMovementConstraintSnapshot();

	return snapshot.monsterMoveNormal == legacy::kServerMonsterMoveNormal &&
		snapshot.monsterMoveStrafe == legacy::kServerMonsterMoveStrafe &&
		NearlyEqual(snapshot.movementEpsilon, legacy::kServerMoveEpsilon) &&
		snapshot.flyMoveClipPlaneLimit == legacy::kServerFlyMoveMaxClipPlanes &&
		snapshot.flyMoveClipIterationLimit ==
			legacy::kServerFlyMoveMaxClipPlanes - 1;
}

static bool TestAdjacentConstantsAreNotMergedByAssumption()
{
	return kServerMoveNormal == legacy::kTraceMoveNormal &&
		kServerMaxClipPlanes == legacy::kPmSharedMaxClipPlanes &&
		!NearlyEqual(kServerMoveEpsilon, legacy::kTraceDistanceEpsilon);
}

}

int main()
{
	if (!TestLegacyValueMirrors() ||
		!TestMonsterMoveTypeHelpers() ||
		!TestFlyMoveClipPlanePolicy() ||
		!TestSnapshot() ||
		!TestAdjacentConstantsAreNotMergedByAssumption())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
