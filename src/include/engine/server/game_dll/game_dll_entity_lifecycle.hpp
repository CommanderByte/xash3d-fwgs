#ifndef XASH_ENGINE_SERVER_GAME_DLL_ENTITY_LIFECYCLE_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_ENTITY_LIFECYCLE_HPP

#include <cstddef>

namespace xash
{
namespace engine
{
namespace server
{

enum class GameDllEntityLookupAction
{
	ReturnNull,
	ReturnEntity,
};

enum class GameDllEdictIndexAction
{
	ReturnWorldIndex,
	ReturnIndex,
	FatalBadEntityNumber,
};

enum class GameDllPrivateDataAllocationAction
{
	FreeExistingOnly,
	AllocateRoundedBlock,
};

struct GameDllEntityLookupPlan
{
	GameDllEntityLookupAction action;
	bool worldSlot;
	bool playerSlot;
};

struct GameDllEdictIndexPlan
{
	GameDllEdictIndexAction action;
	int index;
};

struct GameDllPrivateDataAllocationPlan
{
	GameDllPrivateDataAllocationAction action;
	bool shouldFreeExisting;
	std::size_t roundedBytes;
};

struct GameDllPrivateDataFreePlan
{
	bool shouldCallDestructor;
	bool shouldCheckAndFreeAllocation;
	bool shouldClearPointer;
};

GameDllEntityLookupPlan BuildGameDllEntityLookupPlan(
	int entityIndex,
	int maxEdicts,
	int maxClients,
	bool quakeCompatible,
	bool allEntities,
	bool validEdict,
	bool hasPrivateData);
GameDllEdictIndexPlan BuildGameDllEdictIndexPlan(
	bool edictPresent,
	int computedIndex,
	int maxEdicts);
GameDllPrivateDataAllocationPlan BuildGameDllPrivateDataAllocationPlan(
	long requestedBytes);
GameDllPrivateDataFreePlan BuildGameDllPrivateDataFreePlan(
	bool edictPresent,
	bool privateDataPresent,
	bool destructorAvailable);

const char *GameDllEntityLookupActionName(GameDllEntityLookupAction action);
const char *GameDllEdictIndexActionName(GameDllEdictIndexAction action);
const char *GameDllPrivateDataAllocationActionName(
	GameDllPrivateDataAllocationAction action);

}
}
}

#endif
