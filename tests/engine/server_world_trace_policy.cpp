#include <cstdlib>

#include "engine/server/world/server_world_trace_policy.hpp"

using namespace xash::engine::server;

namespace
{

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
	if (!TestDecodesMoveTypeAndIgnoreTransparentBits() ||
		!TestQuakeCompatibleSuppressesRequestedMonsterClip() ||
		!TestMissileMoveTypeSelectsMissileBounds())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
