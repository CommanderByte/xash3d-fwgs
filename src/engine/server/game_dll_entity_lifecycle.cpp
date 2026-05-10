#include "engine/server/game_dll_entity_lifecycle.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool InEdictRange(int entityIndex, int maxEdicts)
{
	return entityIndex >= 0 && entityIndex < maxEdicts;
}

bool IsLegacyPlayerSlot(int entityIndex, int maxClients, bool allEntities)
{
	return allEntities ? entityIndex <= maxClients : entityIndex < maxClients;
}

}

GameDllEntityLookupPlan BuildGameDllEntityLookupPlan(
	int entityIndex,
	int maxEdicts,
	int maxClients,
	bool quakeCompatible,
	bool allEntities,
	bool validEdict,
	bool hasPrivateData)
{
	GameDllEntityLookupPlan plan = {};

	if (!InEdictRange(entityIndex, maxEdicts))
		return plan;

	plan.worldSlot = entityIndex == 0;
	plan.playerSlot = IsLegacyPlayerSlot(entityIndex, maxClients, allEntities);

	if (plan.worldSlot || quakeCompatible)
	{
		plan.action = GameDllEntityLookupAction::ReturnEntity;
		return plan;
	}

	if (validEdict && hasPrivateData)
	{
		plan.action = GameDllEntityLookupAction::ReturnEntity;
		return plan;
	}

	if (validEdict && plan.playerSlot)
	{
		plan.action = GameDllEntityLookupAction::ReturnEntity;
		return plan;
	}

	return plan;
}

GameDllEdictIndexPlan BuildGameDllEdictIndexPlan(
	bool edictPresent,
	int computedIndex,
	int maxEdicts)
{
	GameDllEdictIndexPlan plan = {};

	if (!edictPresent)
	{
		plan.action = GameDllEdictIndexAction::ReturnWorldIndex;
		return plan;
	}

	plan.index = computedIndex;

	if (computedIndex < 0 || computedIndex > maxEdicts)
	{
		plan.action = GameDllEdictIndexAction::FatalBadEntityNumber;
		return plan;
	}

	plan.action = GameDllEdictIndexAction::ReturnIndex;
	return plan;
}

GameDllPrivateDataAllocationPlan BuildGameDllPrivateDataAllocationPlan(
	long requestedBytes)
{
	GameDllPrivateDataAllocationPlan plan = {};
	plan.action = GameDllPrivateDataAllocationAction::FreeExistingOnly;
	plan.shouldFreeExisting = true;

	if (requestedBytes <= 0)
		return plan;

	plan.action = GameDllPrivateDataAllocationAction::AllocateRoundedBlock;
	const std::size_t bytes = static_cast<std::size_t>(requestedBytes);
	plan.roundedBytes = (bytes + 15U) & ~static_cast<std::size_t>(15U);
	return plan;
}

GameDllPrivateDataFreePlan BuildGameDllPrivateDataFreePlan(
	bool edictPresent,
	bool privateDataPresent,
	bool destructorAvailable)
{
	GameDllPrivateDataFreePlan plan = {};

	if (!edictPresent || !privateDataPresent)
		return plan;

	plan.shouldCallDestructor = destructorAvailable;
	plan.shouldCheckAndFreeAllocation = true;
	plan.shouldClearPointer = true;
	return plan;
}

const char *GameDllEntityLookupActionName(GameDllEntityLookupAction action)
{
	switch (action)
	{
	case GameDllEntityLookupAction::ReturnNull:
		return "return-null";
	case GameDllEntityLookupAction::ReturnEntity:
		return "return-entity";
	}

	return "unknown";
}

const char *GameDllEdictIndexActionName(GameDllEdictIndexAction action)
{
	switch (action)
	{
	case GameDllEdictIndexAction::ReturnWorldIndex:
		return "return-world-index";
	case GameDllEdictIndexAction::ReturnIndex:
		return "return-index";
	case GameDllEdictIndexAction::FatalBadEntityNumber:
		return "fatal-bad-entity-number";
	}

	return "unknown";
}

const char *GameDllPrivateDataAllocationActionName(
	GameDllPrivateDataAllocationAction action)
{
	switch (action)
	{
	case GameDllPrivateDataAllocationAction::FreeExistingOnly:
		return "free-existing-only";
	case GameDllPrivateDataAllocationAction::AllocateRoundedBlock:
		return "allocate-rounded-block";
	}

	return "unknown";
}

}
}
}
