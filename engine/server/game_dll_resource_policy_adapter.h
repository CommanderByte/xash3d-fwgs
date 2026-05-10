#ifndef XASH_ENGINE_SERVER_GAME_DLL_RESOURCE_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_GAME_DLL_RESOURCE_POLICY_ADAPTER_H

#include <stddef.h>

#include "xash3d_types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum sv_gamedll_resource_name_mode_e
{
	SV_GAMEDLL_RESOURCE_MODEL_PRECACHE = 0,
	SV_GAMEDLL_RESOURCE_MODEL_LOOKUP = 1,
	SV_GAMEDLL_RESOURCE_SOUND_PRECACHE = 2,
	SV_GAMEDLL_RESOURCE_GENERIC_PRECACHE = 3,
	SV_GAMEDLL_RESOURCE_EVENT_PRECACHE = 4,
	SV_GAMEDLL_RESOURCE_DECAL_LOOKUP = 5
};

enum sv_gamedll_resource_name_action_e
{
	SV_GAMEDLL_RESOURCE_USE_NAME = 0,
	SV_GAMEDLL_RESOURCE_REJECT_EMPTY = 1,
	SV_GAMEDLL_RESOURCE_REJECT_SENTENCE_NAME = 2
};

enum sv_gamedll_resource_slot_action_e
{
	SV_GAMEDLL_RESOURCE_SLOT_USE = 0,
	SV_GAMEDLL_RESOURCE_SLOT_FATAL_LIMIT_EXCEEDED = 1
};

enum sv_gamedll_model_precache_load_action_e
{
	SV_GAMEDLL_MODEL_PRECACHE_RETURN_ZERO = 0,
	SV_GAMEDLL_MODEL_PRECACHE_LOAD_OPTIONAL = 1,
	SV_GAMEDLL_MODEL_PRECACHE_LOAD_FATAL_IF_MISSING = 2
};

typedef struct sv_gamedll_resource_name_decision_s
{
	int action;
	int optional;
	char normalized_name[MAX_STRING];
} sv_gamedll_resource_name_decision_t;

sv_gamedll_resource_name_decision_t SV_GameDllResource_BuildNameDecision(
	const char *name,
	int mode,
	size_t legacy_capacity);
int SV_GameDllResource_BuildSlotAction(int candidate_index, int max_slots);
int SV_GameDllResource_BuildModelPrecacheLoadAction(
	int optional,
	int model_index);
int SV_GameDllResource_NamesEqual(const char *left, const char *right);

#ifdef __cplusplus
}
#endif

#endif
