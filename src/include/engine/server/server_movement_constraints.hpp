#ifndef XASH_ENGINE_SERVER_SERVER_MOVEMENT_CONSTRAINTS_HPP
#define XASH_ENGINE_SERVER_SERVER_MOVEMENT_CONSTRAINTS_HPP

#include "engine/server/server_limits.hpp"

namespace xash
{
namespace engine
{
namespace server
{

enum class ServerMonsterMoveType
{
	Normal = kServerMoveNormal,
	Strafe = kServerMoveStrafe,
};

struct ServerMovementConstraintSnapshot
{
	int monsterMoveNormal;
	int monsterMoveStrafe;
	float movementEpsilon;
	int flyMoveClipPlaneLimit;
	int flyMoveClipIterationLimit;
};

int ToLegacyMonsterMoveType(ServerMonsterMoveType type);
bool IsServerMonsterNormalMoveType(int legacyMoveType);
bool IsServerMonsterStrafeMoveType(int legacyMoveType);
int ServerFlyMoveClipIterationLimit();
bool ServerFlyMoveCanAddClipPlane(int planeCount);
ServerMovementConstraintSnapshot BuildServerMovementConstraintSnapshot();

}
}
}

#endif
