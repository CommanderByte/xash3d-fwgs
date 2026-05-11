#include "engine/server/game_dll/game_dll_entity_parse.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

const char *SafeText(const char *text)
{
	return text ? text : "";
}

bool EmptyOrNull(const char *text)
{
	return !text || text[0] == '\0';
}

bool Equals(const char *left, const char *right)
{
	return std::strcmp(SafeText(left), right) == 0;
}

std::string TrimTrailingSpaces(const char *text, bool *trimmed)
{
	std::string result = SafeText(text);

	while (!result.empty() && result[result.size() - 1] == ' ')
	{
		result.erase(result.size() - 1);
		if (trimmed)
			*trimmed = true;
	}

	return result;
}

std::string FormatAngles(float pitch, float yaw, float roll)
{
	char buffer[128];
	std::snprintf(
		buffer,
		sizeof(buffer),
		"%g %g %g",
		static_cast<double>(pitch),
		static_cast<double>(yaw),
		static_cast<double>(roll));
	return buffer;
}

float ParseFloat(const char *text)
{
	return static_cast<float>(std::strtod(SafeText(text), nullptr));
}

}

GameDllEntityKeyValuePlan BuildGameDllEntityKeyValuePlan(
	const char *keyName,
	const char *value,
	bool worldSkysphere,
	bool classnameAlreadySeen)
{
	GameDllEntityKeyValuePlan plan = {};

	if (EmptyOrNull(keyName) || EmptyOrNull(value) || Equals(keyName, "wad"))
		return plan;

	if (worldSkysphere && keyName[0] == '_')
		return plan;

	plan.value = SafeText(value);

	if (Equals(keyName, "classname"))
	{
		plan.action = classnameAlreadySeen ?
			GameDllEntityKeyValueAction::SkipDuplicateClassname :
			GameDllEntityKeyValueAction::HandleClassname;
		plan.keyName = "classname";
		return plan;
	}

	plan.action = GameDllEntityKeyValueAction::StoreDeferred;
	plan.keyName = TrimTrailingSpaces(keyName, &plan.trailingSpacesTrimmed);
	return plan;
}

GameDllAngleRewritePlan BuildGameDllAngleRewritePlan(
	const char *keyName,
	const char *value,
	float currentPitch,
	float currentRoll)
{
	GameDllAngleRewritePlan plan = {};
	plan.keyName = SafeText(keyName);
	plan.value = SafeText(value);

	if (!Equals(keyName, "angle"))
		return plan;

	const float yaw = ParseFloat(value);

	plan.rewritten = true;
	plan.keyName = "angles";

	if (yaw >= 0.0f)
		plan.value = FormatAngles(currentPitch, yaw, currentRoll);
	else if (yaw == -1.0f)
		plan.value = "-90 0 0";
	else if (yaw == -2.0f)
		plan.value = "90 0 0";
	else
		plan.value = "0 0 0";

	return plan;
}

GameDllCustomEntityKeyValuePlan BuildGameDllCustomEntityKeyValuePlan(
	bool customEntity,
	const char *classname)
{
	GameDllCustomEntityKeyValuePlan plan = {};

	if (!customEntity)
		return plan;

	plan.shouldSend = true;
	plan.className = "custom";
	plan.keyName = "customclass";
	plan.value = SafeText(classname);
	plan.requireHandled = false;
	return plan;
}

GameDllEntityParsePlan BuildGameDllEntityParsePlan(
	bool classnameSeen,
	bool validAfterAllocation,
	bool killMeAfterAllocation)
{
	GameDllEntityParsePlan plan = {};

	if (!classnameSeen)
	{
		plan.action = GameDllEntityParseAction::RejectMissingClassname;
		plan.shouldFreeDeferredKeyValues = true;
		return plan;
	}

	if (!validAfterAllocation || killMeAfterAllocation)
	{
		plan.action = GameDllEntityParseAction::RejectInvalidEntity;
		plan.shouldFreeDeferredKeyValues = true;
		return plan;
	}

	plan.action = GameDllEntityParseAction::ContinueKeyValues;
	return plan;
}

GameDllEntityLoadAction BuildGameDllEntityLoadAction(
	bool physicsOverrideAvailable,
	bool physicsOverrideHandled)
{
	if (physicsOverrideAvailable && physicsOverrideHandled)
		return GameDllEntityLoadAction::UsePhysicsOverride;

	return GameDllEntityLoadAction::ParseTextEntities;
}

GameDllEntitySpawnPlan BuildGameDllEntitySpawnPlan(
	int spawnResult,
	bool killMeFlag)
{
	GameDllEntitySpawnPlan plan = {};

	if (spawnResult != -1)
	{
		plan.action = GameDllEntitySpawnAction::SpawnAccepted;
		return plan;
	}

	if (killMeFlag)
	{
		plan.action = GameDllEntitySpawnAction::LeaveKillmeEntity;
		return plan;
	}

	plan.action = GameDllEntitySpawnAction::FreeAndInhibit;
	plan.shouldFreeEdict = true;
	plan.shouldIncrementInhibited = true;
	return plan;
}

const char *GameDllEntityKeyValueActionName(
	GameDllEntityKeyValueAction action)
{
	switch (action)
	{
	case GameDllEntityKeyValueAction::Skip:
		return "skip";
	case GameDllEntityKeyValueAction::HandleClassname:
		return "handle-classname";
	case GameDllEntityKeyValueAction::SkipDuplicateClassname:
		return "skip-duplicate-classname";
	case GameDllEntityKeyValueAction::StoreDeferred:
		return "store-deferred";
	}

	return "unknown";
}

const char *GameDllEntityParseActionName(GameDllEntityParseAction action)
{
	switch (action)
	{
	case GameDllEntityParseAction::RejectMissingClassname:
		return "reject-missing-classname";
	case GameDllEntityParseAction::RejectInvalidEntity:
		return "reject-invalid-entity";
	case GameDllEntityParseAction::ContinueKeyValues:
		return "continue-keyvalues";
	}

	return "unknown";
}

const char *GameDllEntityLoadActionName(GameDllEntityLoadAction action)
{
	switch (action)
	{
	case GameDllEntityLoadAction::UsePhysicsOverride:
		return "use-physics-override";
	case GameDllEntityLoadAction::ParseTextEntities:
		return "parse-text-entities";
	}

	return "unknown";
}

const char *GameDllEntitySpawnActionName(GameDllEntitySpawnAction action)
{
	switch (action)
	{
	case GameDllEntitySpawnAction::SpawnAccepted:
		return "spawn-accepted";
	case GameDllEntitySpawnAction::FreeAndInhibit:
		return "free-and-inhibit";
	case GameDllEntitySpawnAction::LeaveKillmeEntity:
		return "leave-killme-entity";
	}

	return "unknown";
}

}
}
}
