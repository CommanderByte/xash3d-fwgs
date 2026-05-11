#include "engine/server/runtime/server_command_lifecycle.hpp"
#include "engine/server/runtime/server_operator_command_policy.hpp"

#include "engine/server/shared/server_map_validation.hpp"

#include <cstdlib>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool IsSeparator(char value)
{
	return value == '/' || value == '\\' || value == ':';
}

std::string StringOrEmpty(const char *value)
{
	return value ? value : "";
}

bool IsAllDigits(const char *value)
{
	if (!value || !*value)
		return false;

	for (const char *cursor = value; *cursor; ++cursor)
	{
		if (*cursor < '0' || *cursor > '9')
			return false;
	}

	return true;
}

std::string StripExtension(std::string value)
{
	if (value.empty())
		return value;

	std::string::size_type index = value.size() - 1;
	while (index > 0 && value[index] != '.')
	{
		--index;
		if (IsSeparator(value[index]))
			return value;
	}

	if (index > 0)
		value.erase(index);

	return value;
}

LifecyclePlan MakePlan(LifecycleAction action)
{
	LifecyclePlan plan = {};
	plan.action = action;
	return plan;
}

LifecyclePlan MakeUsage()
{
	return MakePlan(LifecycleAction::PrintUsage);
}

LifecyclePlan MakeValidateMap(const char *mapArgument, bool background)
{
	LifecyclePlan plan = MakePlan(LifecycleAction::ValidateMap);
	plan.mapName = StripExtension(StringOrEmpty(mapArgument));
	plan.background = background;
	return plan;
}

}

LifecyclePlan BuildMapCommandPlan(
	int argumentCount,
	const char *mapArgument,
	MapLaunchMode mode,
	const BackgroundMapContext &backgroundContext)
{
	if (mode == MapLaunchMode::Background)
	{
		if (backgroundContext.dedicated)
			return MakePlan(LifecycleAction::BackgroundMapDedicatedError);

		if (argumentCount != 2)
			return MakeUsage();

		if (backgroundContext.serverActive && !backgroundContext.alreadyBackground)
		{
			return backgroundContext.nextStateRunFrame
				? MakePlan(LifecycleAction::BackgroundMapActiveError)
				: MakePlan(LifecycleAction::NoOp);
		}

		return MakeValidateMap(mapArgument, true);
	}

	if (argumentCount != 2)
		return MakeUsage();

	return MakeValidateMap(mapArgument, false);
}

MapValidationResult ClassifyMapValidation(unsigned int mapFlags)
{
	switch (ClassifyServerMapValidation(mapFlags))
	{
	case ServerMapValidationResult::InvalidVersion:
		return MapValidationResult::InvalidVersion;
	case ServerMapValidationResult::Missing:
		return MapValidationResult::Missing;
	case ServerMapValidationResult::Valid:
		return MapValidationResult::Valid;
	}

	return MapValidationResult::Valid;
}

LifecyclePlan BuildLoadCommandPlan(int argumentCount, const char *saveArgument)
{
	if (argumentCount != 2)
		return MakeUsage();

	LifecyclePlan plan = MakePlan(LifecycleAction::LoadGame);
	plan.saveName = StringOrEmpty(saveArgument);
	plan.savePath = std::string(kLifecycleSaveDirectory) +
		plan.saveName +
		kLifecycleSaveExtension;
	return plan;
}

LifecyclePlan BuildSaveCommandPlan(int argumentCount, const char *saveArgument)
{
	if (argumentCount == 1)
	{
		LifecyclePlan plan = MakePlan(LifecycleAction::SaveGame);
		plan.saveName = "new";
		return plan;
	}

	if (argumentCount == 2)
	{
		LifecyclePlan plan = MakePlan(LifecycleAction::SaveGame);
		plan.saveName = StringOrEmpty(saveArgument);
		return plan;
	}

	return MakeUsage();
}

LifecyclePlan BuildAutosaveCommandPlan(int argumentCount, bool autosaveEnabled)
{
	if (argumentCount != 1)
		return MakeUsage();

	if (!autosaveEnabled)
		return MakePlan(LifecycleAction::NoOp);

	LifecyclePlan plan = MakePlan(LifecycleAction::SaveGame);
	plan.saveName = "autosave";
	return plan;
}

LifecyclePlan BuildQuickLoadCommandPlan()
{
	LifecyclePlan plan = MakePlan(LifecycleAction::AddCommandText);
	plan.commandText = kQuickLoadCommandText;
	return plan;
}

LifecyclePlan BuildQuickSaveCommandPlan()
{
	LifecyclePlan plan = MakePlan(LifecycleAction::AddCommandText);
	plan.commandText = kQuickSaveCommandText;
	return plan;
}

LifecyclePlan BuildRestartCommandPlan(
	bool serverActive,
	const char *currentMap,
	bool background)
{
	if (!serverActive)
		return MakePlan(LifecycleAction::NoOp);

	LifecyclePlan plan = MakePlan(LifecycleAction::LoadCurrentMap);
	plan.mapName = StringOrEmpty(currentMap);
	plan.background = background;
	return plan;
}

LifecyclePlan BuildReloadCommandPlan(bool nextStateRunFrame)
{
	return nextStateRunFrame
		? MakePlan(LifecycleAction::ReloadLatestSave)
		: MakePlan(LifecycleAction::NoOp);
}

LifecyclePlan BuildChangeLevelCommandPlan(
	bool smoothCommand,
	int argumentCount,
	const char *mapArgument,
	const char *landmarkArgument)
{
	if (argumentCount < 2)
		return MakeUsage();

	LifecyclePlan plan = MakePlan(LifecycleAction::QueueChangeLevel);
	plan.mapName = StringOrEmpty(mapArgument);

	if (smoothCommand && argumentCount > 2)
		plan.landmarkName = StringOrEmpty(landmarkArgument);

	return plan;
}

OperatorInfoCommandPlan BuildOperatorInfoCommandPlan(
	int argumentCount,
	const char *keyArgument,
	const char *valueArgument)
{
	OperatorInfoCommandPlan plan = {};
	plan.key = StringOrEmpty(keyArgument);
	plan.value = StringOrEmpty(valueArgument);

	if (argumentCount == 1)
	{
		plan.action = OperatorInfoCommandAction::PrintCurrent;
		return plan;
	}

	if (argumentCount != 3)
	{
		plan.action = OperatorInfoCommandAction::PrintUsage;
		return plan;
	}

	if (!plan.key.empty() && plan.key[0] == '*')
	{
		plan.action = OperatorInfoCommandAction::RejectStarKey;
		return plan;
	}

	plan.action = OperatorInfoCommandAction::SetValue;
	return plan;
}

OperatorKickCommandPlan BuildOperatorKickCommandPlan(
	int argumentCount,
	const char *targetArgument,
	const char *reasonArgument)
{
	OperatorKickCommandPlan plan = {};
	plan.target = StringOrEmpty(targetArgument);
	plan.reason = StringOrEmpty(reasonArgument);

	if (argumentCount < 2)
	{
		plan.action = OperatorKickCommandAction::PrintUsage;
		return plan;
	}

	if (plan.target.size() > 1 &&
		plan.target[0] == '#' &&
		IsAllDigits(plan.target.c_str() + 1))
	{
		plan.action = OperatorKickCommandAction::FindByUserId;
		plan.userId = std::atoi(plan.target.c_str() + 1);
		return plan;
	}

	plan.action = OperatorKickCommandAction::FindByName;
	return plan;
}

}
}
}
