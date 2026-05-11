#include "engine/server/world/server_movement_constraints.hpp"

namespace xash
{
namespace engine
{
namespace server
{

int ToLegacyMonsterMoveType(ServerMonsterMoveType type)
{
	return static_cast<int>(type);
}

bool IsServerMonsterNormalMoveType(int legacyMoveType)
{
	return legacyMoveType == kServerMoveNormal;
}

bool IsServerMonsterStrafeMoveType(int legacyMoveType)
{
	return legacyMoveType == kServerMoveStrafe;
}

int ServerFlyMoveClipIterationLimit()
{
	return kServerMaxClipPlanes - 1;
}

bool ServerFlyMoveCanAddClipPlane(int planeCount)
{
	return planeCount >= 0 && planeCount < kServerMaxClipPlanes;
}

ServerMovementConstraintSnapshot BuildServerMovementConstraintSnapshot()
{
	ServerMovementConstraintSnapshot snapshot = {};
	snapshot.monsterMoveNormal = kServerMoveNormal;
	snapshot.monsterMoveStrafe = kServerMoveStrafe;
	snapshot.movementEpsilon = kServerMoveEpsilon;
	snapshot.flyMoveClipPlaneLimit = kServerMaxClipPlanes;
	snapshot.flyMoveClipIterationLimit = ServerFlyMoveClipIterationLimit();
	return snapshot;
}

}
}
}
