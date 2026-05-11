#ifndef XASH_ENGINE_SERVER_WORLD_TRACE_POLICY_HPP
#define XASH_ENGINE_SERVER_WORLD_TRACE_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

struct ServerWorldMoveClipPlan
{
	int moveType;
	int ignoreTransparent;
	bool monsterClip;
	bool useMissileBounds;
};

ServerWorldMoveClipPlan BuildServerWorldMoveClipPlan(
	int encodedMoveType,
	bool requestedMonsterClip,
	bool quakeCompatible,
	int missileMoveType);

}
}
}

#endif
