#ifndef XASH_ENGINE_SERVER_GAME_DLL_ENTITY_LIFECYCLE_ADAPTER_H
#define XASH_ENGINE_SERVER_GAME_DLL_ENTITY_LIFECYCLE_ADAPTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum sv_gamedll_entity_lookup_action_e
{
	SV_GAMEDLL_ENTITY_LOOKUP_RETURN_NULL = 0,
	SV_GAMEDLL_ENTITY_LOOKUP_RETURN_ENTITY = 1
};

enum sv_gamedll_edict_index_action_e
{
	SV_GAMEDLL_EDICT_INDEX_RETURN_WORLD_INDEX = 0,
	SV_GAMEDLL_EDICT_INDEX_RETURN_INDEX = 1,
	SV_GAMEDLL_EDICT_INDEX_FATAL_BAD_ENTITY_NUMBER = 2
};

enum sv_gamedll_private_data_allocation_action_e
{
	SV_GAMEDLL_PRIVATE_DATA_FREE_EXISTING_ONLY = 0,
	SV_GAMEDLL_PRIVATE_DATA_ALLOCATE_ROUNDED_BLOCK = 1
};

typedef struct sv_gamedll_entity_lookup_plan_s
{
	int action;
	int world_slot;
	int player_slot;
} sv_gamedll_entity_lookup_plan_t;

typedef struct sv_gamedll_edict_index_plan_s
{
	int action;
	int index;
} sv_gamedll_edict_index_plan_t;

typedef struct sv_gamedll_private_data_allocation_plan_s
{
	int action;
	int should_free_existing;
	size_t rounded_bytes;
} sv_gamedll_private_data_allocation_plan_t;

typedef struct sv_gamedll_private_data_free_plan_s
{
	int should_call_destructor;
	int should_check_and_free_allocation;
	int should_clear_pointer;
} sv_gamedll_private_data_free_plan_t;

sv_gamedll_entity_lookup_plan_t SV_GameDllEntity_BuildLookupPlan(
	int entity_index,
	int max_edicts,
	int max_clients,
	int quake_compatible,
	int all_entities,
	int valid_edict,
	int has_private_data);
sv_gamedll_edict_index_plan_t SV_GameDllEntity_BuildEdictIndexPlan(
	int edict_present,
	int computed_index,
	int max_edicts);
sv_gamedll_private_data_allocation_plan_t
SV_GameDllEntity_BuildPrivateDataAllocationPlan(long requested_bytes);
sv_gamedll_private_data_free_plan_t
SV_GameDllEntity_BuildPrivateDataFreePlan(
	int edict_present,
	int private_data_present,
	int destructor_available);

#ifdef __cplusplus
}
#endif

#endif
