#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll/game_dll_entity_lifecycle.hpp"

using namespace xash::engine::server;

namespace
{

static bool TestEntityIndexLookupRange()
{
	return BuildGameDllEntityLookupPlan(
			-1, 64, 4, false, true, true, true).action ==
			GameDllEntityLookupAction::ReturnNull &&
		BuildGameDllEntityLookupPlan(
			64, 64, 4, false, true, true, true).action ==
			GameDllEntityLookupAction::ReturnNull;
}

static bool TestWorldAndQuakeCompatibleLookup()
{
	const GameDllEntityLookupPlan world =
		BuildGameDllEntityLookupPlan(0, 64, 4, false, false, false, false);
	const GameDllEntityLookupPlan quake =
		BuildGameDllEntityLookupPlan(9, 64, 4, true, false, false, false);

	return world.action == GameDllEntityLookupAction::ReturnEntity &&
		world.worldSlot &&
		world.playerSlot &&
		quake.action == GameDllEntityLookupAction::ReturnEntity;
}

static bool TestClientVisibleVersusAllEntitiesLookup()
{
	const GameDllEntityLookupPlan firstClient =
		BuildGameDllEntityLookupPlan(1, 64, 4, false, false, true, false);
	const GameDllEntityLookupPlan lastClientBugCompat =
		BuildGameDllEntityLookupPlan(4, 64, 4, false, false, true, false);
	const GameDllEntityLookupPlan lastClientAllEntities =
		BuildGameDllEntityLookupPlan(4, 64, 4, false, true, true, false);

	return firstClient.action == GameDllEntityLookupAction::ReturnEntity &&
		firstClient.playerSlot &&
		lastClientBugCompat.action == GameDllEntityLookupAction::ReturnNull &&
		!lastClientBugCompat.playerSlot &&
		lastClientAllEntities.action ==
			GameDllEntityLookupAction::ReturnEntity &&
		lastClientAllEntities.playerSlot;
}

static bool TestPrivateDataEntityLookup()
{
	const GameDllEntityLookupPlan privateDataEntity =
		BuildGameDllEntityLookupPlan(12, 64, 4, false, false, true, true);
	const GameDllEntityLookupPlan freeEntity =
		BuildGameDllEntityLookupPlan(12, 64, 4, false, false, false, true);

	return privateDataEntity.action == GameDllEntityLookupAction::ReturnEntity &&
		freeEntity.action == GameDllEntityLookupAction::ReturnNull;
}

static bool TestEdictIndexPlans()
{
	const GameDllEdictIndexPlan nullEdict =
		BuildGameDllEdictIndexPlan(false, 99, 64);
	const GameDllEdictIndexPlan valid =
		BuildGameDllEdictIndexPlan(true, 12, 64);
	const GameDllEdictIndexPlan onePast =
		BuildGameDllEdictIndexPlan(true, 64, 64);
	const GameDllEdictIndexPlan low =
		BuildGameDllEdictIndexPlan(true, -1, 64);
	const GameDllEdictIndexPlan high =
		BuildGameDllEdictIndexPlan(true, 65, 64);

	return nullEdict.action == GameDllEdictIndexAction::ReturnWorldIndex &&
		nullEdict.index == 0 &&
		valid.action == GameDllEdictIndexAction::ReturnIndex &&
		valid.index == 12 &&
		onePast.action == GameDllEdictIndexAction::ReturnIndex &&
		onePast.index == 64 &&
		low.action == GameDllEdictIndexAction::FatalBadEntityNumber &&
		high.action == GameDllEdictIndexAction::FatalBadEntityNumber;
}

static bool TestPrivateDataAllocationPlans()
{
	const GameDllPrivateDataAllocationPlan zero =
		BuildGameDllPrivateDataAllocationPlan(0);
	const GameDllPrivateDataAllocationPlan negative =
		BuildGameDllPrivateDataAllocationPlan(-7);
	const GameDllPrivateDataAllocationPlan one =
		BuildGameDllPrivateDataAllocationPlan(1);
	const GameDllPrivateDataAllocationPlan exact =
		BuildGameDllPrivateDataAllocationPlan(16);
	const GameDllPrivateDataAllocationPlan next =
		BuildGameDllPrivateDataAllocationPlan(17);

	return zero.action ==
			GameDllPrivateDataAllocationAction::FreeExistingOnly &&
		zero.shouldFreeExisting &&
		zero.roundedBytes == 0 &&
		negative.action ==
			GameDllPrivateDataAllocationAction::FreeExistingOnly &&
		one.action ==
			GameDllPrivateDataAllocationAction::AllocateRoundedBlock &&
		one.roundedBytes == 16 &&
		exact.roundedBytes == 16 &&
		next.roundedBytes == 32;
}

static bool TestPrivateDataFreePlans()
{
	const GameDllPrivateDataFreePlan noEdict =
		BuildGameDllPrivateDataFreePlan(false, true, true);
	const GameDllPrivateDataFreePlan noPrivateData =
		BuildGameDllPrivateDataFreePlan(true, false, true);
	const GameDllPrivateDataFreePlan withoutDestructor =
		BuildGameDllPrivateDataFreePlan(true, true, false);
	const GameDllPrivateDataFreePlan withDestructor =
		BuildGameDllPrivateDataFreePlan(true, true, true);

	return !noEdict.shouldClearPointer &&
		!noPrivateData.shouldClearPointer &&
		withoutDestructor.shouldCheckAndFreeAllocation &&
		withoutDestructor.shouldClearPointer &&
		!withoutDestructor.shouldCallDestructor &&
		withDestructor.shouldCallDestructor &&
		withDestructor.shouldCheckAndFreeAllocation &&
		withDestructor.shouldClearPointer;
}

static bool TestDisplayNames()
{
	return std::strcmp(
			GameDllEntityLookupActionName(
				GameDllEntityLookupAction::ReturnEntity),
			"return-entity") == 0 &&
		std::strcmp(
			GameDllEdictIndexActionName(
				GameDllEdictIndexAction::FatalBadEntityNumber),
			"fatal-bad-entity-number") == 0 &&
		std::strcmp(
			GameDllPrivateDataAllocationActionName(
				GameDllPrivateDataAllocationAction::AllocateRoundedBlock),
			"allocate-rounded-block") == 0;
}

}

int main()
{
	if (!TestEntityIndexLookupRange() ||
		!TestWorldAndQuakeCompatibleLookup() ||
		!TestClientVisibleVersusAllEntitiesLookup() ||
		!TestPrivateDataEntityLookup() ||
		!TestEdictIndexPlans() ||
		!TestPrivateDataAllocationPlans() ||
		!TestPrivateDataFreePlans() ||
		!TestDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
