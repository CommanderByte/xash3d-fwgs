#include "engine/server/game_dll_resource_policy.hpp"

#include <cctype>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool EmptyOrNull(const char *value)
{
	return !value || value[0] == '\0';
}

bool ShouldStripOptionalPrefix(GameDllResourceNameMode mode)
{
	return mode == GameDllResourceNameMode::ModelPrecache;
}

bool ShouldRejectSentenceName(GameDllResourceNameMode mode)
{
	return mode == GameDllResourceNameMode::SoundPrecache;
}

bool ShouldStripLeadingSlash(GameDllResourceNameMode mode)
{
	return mode == GameDllResourceNameMode::ModelPrecache ||
		mode == GameDllResourceNameMode::ModelLookup ||
		mode == GameDllResourceNameMode::SoundPrecache;
}

bool ShouldNormalizeSlashes(GameDllResourceNameMode mode)
{
	return mode != GameDllResourceNameMode::DecalLookup;
}

bool IsSlash(char ch)
{
	return ch == '/' || ch == '\\';
}

std::string CopyWithLegacyCapacity(const char *text, std::size_t capacity)
{
	if (capacity == 0)
		return text;

	const std::size_t maxLength = capacity > 0 ? capacity - 1 : 0;
	return std::string(text).substr(0, maxLength);
}

void NormalizeSlashes(std::string &name)
{
	for (char &ch : name)
	{
		if (ch == '\\')
			ch = '/';
	}

	std::string compact;
	compact.reserve(name.size());

	for (std::size_t i = 0; i < name.size(); ++i)
	{
		if (name[i] == '/' && i + 1 < name.size() && name[i + 1] == '/')
			continue;

		compact.push_back(name[i]);
	}

	name.swap(compact);
}

char LowerAscii(char ch)
{
	return static_cast<char>(
		std::tolower(static_cast<unsigned char>(ch)));
}

}

GameDllResourceNameDecision BuildGameDllResourceNameDecision(
	const char *name,
	GameDllResourceNameMode mode,
	std::size_t legacyCapacity)
{
	GameDllResourceNameDecision decision = {};

	if (EmptyOrNull(name))
	{
		decision.action = GameDllResourceNameAction::RejectEmpty;
		return decision;
	}

	if (ShouldRejectSentenceName(mode) && name[0] == '!')
	{
		decision.action = GameDllResourceNameAction::RejectSentenceName;
		return decision;
	}

	if (ShouldStripOptionalPrefix(mode) && name[0] == '!')
	{
		decision.optional = true;
		++name;
	}

	if (ShouldStripLeadingSlash(mode) && IsSlash(name[0]))
		++name;

	decision.action = GameDllResourceNameAction::UseName;
	decision.normalizedName = CopyWithLegacyCapacity(name, legacyCapacity);

	if (ShouldNormalizeSlashes(mode))
		NormalizeSlashes(decision.normalizedName);

	return decision;
}

GameDllResourceSlotAction BuildGameDllResourceSlotAction(
	int candidateIndex,
	int maxSlots)
{
	if (candidateIndex >= maxSlots)
		return GameDllResourceSlotAction::FatalLimitExceeded;

	return GameDllResourceSlotAction::UseSlot;
}

GameDllModelPrecacheLoadAction BuildGameDllModelPrecacheLoadAction(
	bool optional,
	int modelIndex)
{
	if (modelIndex <= 0)
		return GameDllModelPrecacheLoadAction::ReturnZero;

	if (optional)
		return GameDllModelPrecacheLoadAction::LoadOptional;

	return GameDllModelPrecacheLoadAction::LoadFatalIfMissing;
}

bool GameDllResourceNamesEqual(const char *left, const char *right)
{
	if (!left || !right)
		return left == right;

	while (*left && *right)
	{
		if (LowerAscii(*left) != LowerAscii(*right))
			return false;

		++left;
		++right;
	}

	return *left == *right;
}

const char *GameDllResourceNameActionName(GameDllResourceNameAction action)
{
	switch (action)
	{
	case GameDllResourceNameAction::UseName:
		return "use-name";
	case GameDllResourceNameAction::RejectEmpty:
		return "reject-empty";
	case GameDllResourceNameAction::RejectSentenceName:
		return "reject-sentence-name";
	}

	return "unknown";
}

const char *GameDllResourceSlotActionName(GameDllResourceSlotAction action)
{
	switch (action)
	{
	case GameDllResourceSlotAction::UseSlot:
		return "use-slot";
	case GameDllResourceSlotAction::FatalLimitExceeded:
		return "fatal-limit-exceeded";
	}

	return "unknown";
}

const char *GameDllModelPrecacheLoadActionName(
	GameDllModelPrecacheLoadAction action)
{
	switch (action)
	{
	case GameDllModelPrecacheLoadAction::ReturnZero:
		return "return-zero";
	case GameDllModelPrecacheLoadAction::LoadOptional:
		return "load-optional";
	case GameDllModelPrecacheLoadAction::LoadFatalIfMissing:
		return "load-fatal-if-missing";
	}

	return "unknown";
}

}
}
}
