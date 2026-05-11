#include "engine/server/server_world_trace_policy.hpp"

namespace xash
{
namespace engine
{
namespace server
{

ServerWorldMoveClipPlan BuildServerWorldMoveClipPlan(
	int encodedMoveType,
	bool requestedMonsterClip,
	bool quakeCompatible,
	int missileMoveType)
{
	ServerWorldMoveClipPlan plan = {};
	plan.moveType = encodedMoveType & 0xff;
	plan.ignoreTransparent = encodedMoveType >> 8;
	plan.monsterClip = requestedMonsterClip && !quakeCompatible;
	plan.useMissileBounds = plan.moveType == missileMoveType;
	return plan;
}

}
}
}
