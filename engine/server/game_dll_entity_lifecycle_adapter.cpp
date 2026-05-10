#include "game_dll_entity_lifecycle_adapter.h"

#include "engine/server/game_dll_entity_lifecycle.hpp"

namespace
{

int ToLegacyLookupAction(
	xash::engine::server::GameDllEntityLookupAction action)
{
	using xash::engine::server::GameDllEntityLookupAction;

	switch (action)
	{
	case GameDllEntityLookupAction::ReturnEntity:
		return SV_GAMEDLL_ENTITY_LOOKUP_RETURN_ENTITY;
	case GameDllEntityLookupAction::ReturnNull:
	default:
		return SV_GAMEDLL_ENTITY_LOOKUP_RETURN_NULL;
	}
}

int ToLegacyEdictIndexAction(
	xash::engine::server::GameDllEdictIndexAction action)
{
	using xash::engine::server::GameDllEdictIndexAction;

	switch (action)
	{
	case GameDllEdictIndexAction::ReturnWorldIndex:
		return SV_GAMEDLL_EDICT_INDEX_RETURN_WORLD_INDEX;
	case GameDllEdictIndexAction::ReturnIndex:
		return SV_GAMEDLL_EDICT_INDEX_RETURN_INDEX;
	case GameDllEdictIndexAction::FatalBadEntityNumber:
	default:
		return SV_GAMEDLL_EDICT_INDEX_FATAL_BAD_ENTITY_NUMBER;
	}
}

int ToLegacyPrivateDataAllocationAction(
	xash::engine::server::GameDllPrivateDataAllocationAction action)
{
	using xash::engine::server::GameDllPrivateDataAllocationAction;

	switch (action)
	{
	case GameDllPrivateDataAllocationAction::AllocateRoundedBlock:
		return SV_GAMEDLL_PRIVATE_DATA_ALLOCATE_ROUNDED_BLOCK;
	case GameDllPrivateDataAllocationAction::FreeExistingOnly:
	default:
		return SV_GAMEDLL_PRIVATE_DATA_FREE_EXISTING_ONLY;
	}
}

}

extern "C" sv_gamedll_entity_lookup_plan_t
SV_GameDllEntity_BuildLookupPlan(
	int entity_index,
	int max_edicts,
	int max_clients,
	int quake_compatible,
	int all_entities,
	int valid_edict,
	int has_private_data)
{
	const xash::engine::server::GameDllEntityLookupPlan modern =
		xash::engine::server::BuildGameDllEntityLookupPlan(
			entity_index,
			max_edicts,
			max_clients,
			quake_compatible != 0,
			all_entities != 0,
			valid_edict != 0,
			has_private_data != 0);

	sv_gamedll_entity_lookup_plan_t legacy = {};
	legacy.action = ToLegacyLookupAction(modern.action);
	legacy.world_slot = modern.worldSlot ? 1 : 0;
	legacy.player_slot = modern.playerSlot ? 1 : 0;
	return legacy;
}

extern "C" sv_gamedll_edict_index_plan_t
SV_GameDllEntity_BuildEdictIndexPlan(
	int edict_present,
	int computed_index,
	int max_edicts)
{
	const xash::engine::server::GameDllEdictIndexPlan modern =
		xash::engine::server::BuildGameDllEdictIndexPlan(
			edict_present != 0,
			computed_index,
			max_edicts);

	sv_gamedll_edict_index_plan_t legacy = {};
	legacy.action = ToLegacyEdictIndexAction(modern.action);
	legacy.index = modern.index;
	return legacy;
}

extern "C" sv_gamedll_private_data_allocation_plan_t
SV_GameDllEntity_BuildPrivateDataAllocationPlan(long requested_bytes)
{
	const xash::engine::server::GameDllPrivateDataAllocationPlan modern =
		xash::engine::server::BuildGameDllPrivateDataAllocationPlan(
			requested_bytes);

	sv_gamedll_private_data_allocation_plan_t legacy = {};
	legacy.action = ToLegacyPrivateDataAllocationAction(modern.action);
	legacy.should_free_existing = modern.shouldFreeExisting ? 1 : 0;
	legacy.rounded_bytes = modern.roundedBytes;
	return legacy;
}

extern "C" sv_gamedll_private_data_free_plan_t
SV_GameDllEntity_BuildPrivateDataFreePlan(
	int edict_present,
	int private_data_present,
	int destructor_available)
{
	const xash::engine::server::GameDllPrivateDataFreePlan modern =
		xash::engine::server::BuildGameDllPrivateDataFreePlan(
			edict_present != 0,
			private_data_present != 0,
			destructor_available != 0);

	sv_gamedll_private_data_free_plan_t legacy = {};
	legacy.should_call_destructor = modern.shouldCallDestructor ? 1 : 0;
	legacy.should_check_and_free_allocation =
		modern.shouldCheckAndFreeAllocation ? 1 : 0;
	legacy.should_clear_pointer = modern.shouldClearPointer ? 1 : 0;
	return legacy;
}
