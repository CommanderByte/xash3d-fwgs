#ifndef XASH_ENGINE_SERVER_GAME_DLL_RESOURCE_POLICY_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_RESOURCE_POLICY_HPP

#include <cstddef>
#include <string>

namespace xash
{
namespace engine
{
namespace server
{

enum class GameDllResourceNameMode
{
	ModelPrecache,
	ModelLookup,
	SoundPrecache,
	GenericPrecache,
	EventPrecache,
	DecalLookup,
};

enum class GameDllResourceNameAction
{
	UseName,
	RejectEmpty,
	RejectSentenceName,
};

enum class GameDllResourceSlotAction
{
	UseSlot,
	FatalLimitExceeded,
};

enum class GameDllModelPrecacheLoadAction
{
	ReturnZero,
	LoadOptional,
	LoadFatalIfMissing,
};

struct GameDllResourceNameDecision
{
	GameDllResourceNameAction action;
	bool optional;
	std::string normalizedName;
};

GameDllResourceNameDecision BuildGameDllResourceNameDecision(
	const char *name,
	GameDllResourceNameMode mode,
	std::size_t legacyCapacity);
GameDllResourceSlotAction BuildGameDllResourceSlotAction(
	int candidateIndex,
	int maxSlots);
GameDllModelPrecacheLoadAction BuildGameDllModelPrecacheLoadAction(
	bool optional,
	int modelIndex);
bool GameDllResourceNamesEqual(const char *left, const char *right);

const char *GameDllResourceNameActionName(GameDllResourceNameAction action);
const char *GameDllResourceSlotActionName(GameDllResourceSlotAction action);
const char *GameDllModelPrecacheLoadActionName(
	GameDllModelPrecacheLoadAction action);

}
}
}

#endif
