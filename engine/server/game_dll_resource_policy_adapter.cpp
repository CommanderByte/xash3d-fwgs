#include "game_dll_resource_policy_adapter.h"

#include "engine/server/game_dll_resource_policy.hpp"

#include <cstdio>

namespace
{

xash::engine::server::GameDllResourceNameMode ToModernNameMode(int mode)
{
	using xash::engine::server::GameDllResourceNameMode;

	switch (mode)
	{
	case SV_GAMEDLL_RESOURCE_MODEL_PRECACHE:
		return GameDllResourceNameMode::ModelPrecache;
	case SV_GAMEDLL_RESOURCE_MODEL_LOOKUP:
		return GameDllResourceNameMode::ModelLookup;
	case SV_GAMEDLL_RESOURCE_SOUND_PRECACHE:
		return GameDllResourceNameMode::SoundPrecache;
	case SV_GAMEDLL_RESOURCE_GENERIC_PRECACHE:
		return GameDllResourceNameMode::GenericPrecache;
	case SV_GAMEDLL_RESOURCE_EVENT_PRECACHE:
		return GameDllResourceNameMode::EventPrecache;
	case SV_GAMEDLL_RESOURCE_DECAL_LOOKUP:
	default:
		return GameDllResourceNameMode::DecalLookup;
	}
}

int ToLegacyAction(xash::engine::server::GameDllResourceNameAction action)
{
	using xash::engine::server::GameDllResourceNameAction;

	switch (action)
	{
	case GameDllResourceNameAction::UseName:
		return SV_GAMEDLL_RESOURCE_USE_NAME;
	case GameDllResourceNameAction::RejectSentenceName:
		return SV_GAMEDLL_RESOURCE_REJECT_SENTENCE_NAME;
	case GameDllResourceNameAction::RejectEmpty:
	default:
		return SV_GAMEDLL_RESOURCE_REJECT_EMPTY;
	}
}

int ToLegacySlotAction(xash::engine::server::GameDllResourceSlotAction action)
{
	using xash::engine::server::GameDllResourceSlotAction;

	switch (action)
	{
	case GameDllResourceSlotAction::UseSlot:
		return SV_GAMEDLL_RESOURCE_SLOT_USE;
	case GameDllResourceSlotAction::FatalLimitExceeded:
	default:
		return SV_GAMEDLL_RESOURCE_SLOT_FATAL_LIMIT_EXCEEDED;
	}
}

int ToLegacyLoadAction(
	xash::engine::server::GameDllModelPrecacheLoadAction action)
{
	using xash::engine::server::GameDllModelPrecacheLoadAction;

	switch (action)
	{
	case GameDllModelPrecacheLoadAction::LoadOptional:
		return SV_GAMEDLL_MODEL_PRECACHE_LOAD_OPTIONAL;
	case GameDllModelPrecacheLoadAction::LoadFatalIfMissing:
		return SV_GAMEDLL_MODEL_PRECACHE_LOAD_FATAL_IF_MISSING;
	case GameDllModelPrecacheLoadAction::ReturnZero:
	default:
		return SV_GAMEDLL_MODEL_PRECACHE_RETURN_ZERO;
	}
}

void CopyString(char *dst, std::size_t capacity, const std::string &src)
{
	if (!dst || capacity == 0)
		return;

	std::snprintf(dst, capacity, "%s", src.c_str());
}

}

extern "C" sv_gamedll_resource_name_decision_t
SV_GameDllResource_BuildNameDecision(
	const char *name,
	int mode,
	size_t legacy_capacity)
{
	const xash::engine::server::GameDllResourceNameDecision modern =
		xash::engine::server::BuildGameDllResourceNameDecision(
			name,
			ToModernNameMode(mode),
			legacy_capacity);

	sv_gamedll_resource_name_decision_t legacy = {};
	legacy.action = ToLegacyAction(modern.action);
	legacy.optional = modern.optional ? 1 : 0;
	CopyString(
		legacy.normalized_name,
		sizeof(legacy.normalized_name),
		modern.normalizedName);
	return legacy;
}

extern "C" int SV_GameDllResource_BuildSlotAction(
	int candidate_index,
	int max_slots)
{
	return ToLegacySlotAction(
		xash::engine::server::BuildGameDllResourceSlotAction(
			candidate_index,
			max_slots));
}

extern "C" int SV_GameDllResource_BuildModelPrecacheLoadAction(
	int optional,
	int model_index)
{
	return ToLegacyLoadAction(
		xash::engine::server::BuildGameDllModelPrecacheLoadAction(
			optional != 0,
			model_index));
}

extern "C" int SV_GameDllResource_NamesEqual(
	const char *left,
	const char *right)
{
	return xash::engine::server::GameDllResourceNamesEqual(left, right) ? 1 : 0;
}
