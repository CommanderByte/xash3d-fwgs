#ifndef XASH_ENGINE_SERVER_WORLD_TRACE_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_WORLD_TRACE_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_world_move_clip_plan_s
{
	int move_type;
	int ignore_transparent;
	int monsterclip;
	int use_missile_bounds;
} sv_world_move_clip_plan_t;

sv_world_move_clip_plan_t SV_WorldTrace_BuildMoveClipPlan(
	int encoded_move_type,
	int requested_monsterclip,
	int quake_compatible,
	int missile_move_type);

#ifdef __cplusplus
}
#endif

#endif
