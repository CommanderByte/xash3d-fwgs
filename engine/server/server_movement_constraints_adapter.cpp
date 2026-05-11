#include "server_movement_constraints_adapter.h"

#include "engine/server/world/server_movement_constraints.hpp"

extern "C" int SV_MovementConstraints_IsMonsterNormalMoveType(int move_type)
{
	return xash::engine::server::IsServerMonsterNormalMoveType(move_type) ? 1 : 0;
}

extern "C" int SV_MovementConstraints_IsMonsterStrafeMoveType(int move_type)
{
	return xash::engine::server::IsServerMonsterStrafeMoveType(move_type) ? 1 : 0;
}
