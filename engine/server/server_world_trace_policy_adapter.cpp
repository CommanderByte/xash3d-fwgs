#include "server_world_trace_policy_adapter.h"

#include "engine/server/world/server_world_trace_policy.hpp"

extern "C" sv_world_move_clip_plan_t SV_WorldTrace_BuildMoveClipPlan(
	int encoded_move_type,
	int requested_monsterclip,
	int quake_compatible,
	int missile_move_type)
{
	const xash::engine::server::ServerWorldMoveClipPlan plan =
		xash::engine::server::BuildServerWorldMoveClipPlan(
			encoded_move_type,
			requested_monsterclip != 0,
			quake_compatible != 0,
			missile_move_type);

	sv_world_move_clip_plan_t legacy = {};
	legacy.move_type = plan.moveType;
	legacy.ignore_transparent = plan.ignoreTransparent;
	legacy.monsterclip = plan.monsterClip ? 1 : 0;
	legacy.use_missile_bounds = plan.useMissileBounds ? 1 : 0;
	return legacy;
}
