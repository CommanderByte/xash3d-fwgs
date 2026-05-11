#include <cstdlib>

#include "engine/server/runtime/server_command_lifecycle.hpp"
#include "engine/server/runtime/server_operator_command_policy.hpp"

using namespace xash::engine::server;

namespace
{

static BackgroundMapContext BackgroundContext()
{
	BackgroundMapContext context = {};
	context.dedicated = false;
	context.serverActive = false;
	context.alreadyBackground = false;
	context.nextStateRunFrame = true;
	return context;
}

static bool TestMapCommandRequiresOneArgumentAndStripsExtension()
{
	const LifecyclePlan usage = BuildMapCommandPlan(
		1,
		nullptr,
		MapLaunchMode::Foreground,
		BackgroundContext());
	const LifecyclePlan plan = BuildMapCommandPlan(
		2,
		"c1a0.bsp",
		MapLaunchMode::Foreground,
		BackgroundContext());

	return usage.action == LifecycleAction::PrintUsage &&
		plan.action == LifecycleAction::ValidateMap &&
		plan.mapName == "c1a0" &&
		!plan.background;
}

static bool TestMapCommandKeepsDotsBeforePathSeparator()
{
	const LifecyclePlan plan = BuildMapCommandPlan(
		2,
		"maps.v1/c1a0",
		MapLaunchMode::Foreground,
		BackgroundContext());

	return plan.action == LifecycleAction::ValidateMap &&
		plan.mapName == "maps.v1/c1a0";
}

static bool TestBackgroundMapDedicatedTakesPrecedence()
{
	BackgroundMapContext context = BackgroundContext();
	context.dedicated = true;

	const LifecyclePlan plan = BuildMapCommandPlan(
		1,
		nullptr,
		MapLaunchMode::Background,
		context);

	return plan.action == LifecycleAction::BackgroundMapDedicatedError;
}

static bool TestBackgroundMapUsageAndActiveRejection()
{
	const LifecyclePlan usage = BuildMapCommandPlan(
		1,
		nullptr,
		MapLaunchMode::Background,
		BackgroundContext());

	BackgroundMapContext active = BackgroundContext();
	active.serverActive = true;
	active.alreadyBackground = false;
	active.nextStateRunFrame = true;
	const LifecyclePlan rejected = BuildMapCommandPlan(
		2,
		"background01.bsp",
		MapLaunchMode::Background,
		active);

	active.nextStateRunFrame = false;
	const LifecyclePlan silent = BuildMapCommandPlan(
		2,
		"background01.bsp",
		MapLaunchMode::Background,
		active);

	return usage.action == LifecycleAction::PrintUsage &&
		rejected.action == LifecycleAction::BackgroundMapActiveError &&
		silent.action == LifecycleAction::NoOp;
}

static bool TestBackgroundMapCanReplaceExistingBackground()
{
	BackgroundMapContext context = BackgroundContext();
	context.serverActive = true;
	context.alreadyBackground = true;

	const LifecyclePlan plan = BuildMapCommandPlan(
		2,
		"background02.bsp",
		MapLaunchMode::Background,
		context);

	return plan.action == LifecycleAction::ValidateMap &&
		plan.mapName == "background02" &&
		plan.background;
}

static bool TestMapValidationClassification()
{
	return ClassifyMapValidation(kLifecycleMapExists) == MapValidationResult::Valid &&
		ClassifyMapValidation(kLifecycleMapInvalidVersion) ==
			MapValidationResult::InvalidVersion &&
		ClassifyMapValidation(0) == MapValidationResult::Missing &&
		ClassifyMapValidation(
			kLifecycleMapExists | kLifecycleMapInvalidVersion) ==
			MapValidationResult::InvalidVersion;
}

static bool TestLoadCommandBuildsLegacyPath()
{
	const LifecyclePlan usage = BuildLoadCommandPlan(1, nullptr);
	const LifecyclePlan plan = BuildLoadCommandPlan(2, "quick");

	return usage.action == LifecycleAction::PrintUsage &&
		plan.action == LifecycleAction::LoadGame &&
		plan.saveName == "quick" &&
		plan.savePath == "save/quick.sav";
}

static bool TestSaveCommandDefaultAndExplicitNames()
{
	const LifecyclePlan defaultSave = BuildSaveCommandPlan(1, nullptr);
	const LifecyclePlan explicitSave = BuildSaveCommandPlan(2, "chapter1");
	const LifecyclePlan usage = BuildSaveCommandPlan(3, "too-many");

	return defaultSave.action == LifecycleAction::SaveGame &&
		defaultSave.saveName == "new" &&
		explicitSave.action == LifecycleAction::SaveGame &&
		explicitSave.saveName == "chapter1" &&
		usage.action == LifecycleAction::PrintUsage;
}

static bool TestAutosaveCommand()
{
	const LifecyclePlan usage = BuildAutosaveCommandPlan(2, true);
	const LifecyclePlan disabled = BuildAutosaveCommandPlan(1, false);
	const LifecyclePlan enabled = BuildAutosaveCommandPlan(1, true);

	return usage.action == LifecycleAction::PrintUsage &&
		disabled.action == LifecycleAction::NoOp &&
		enabled.action == LifecycleAction::SaveGame &&
		enabled.saveName == "autosave";
}

static bool TestQuickAliases()
{
	const LifecyclePlan load = BuildQuickLoadCommandPlan();
	const LifecyclePlan save = BuildQuickSaveCommandPlan();

	return load.action == LifecycleAction::AddCommandText &&
		load.commandText == kQuickLoadCommandText &&
		save.action == LifecycleAction::AddCommandText &&
		save.commandText == kQuickSaveCommandText;
}

static bool TestRestartCommand()
{
	const LifecyclePlan stopped = BuildRestartCommandPlan(false, "c1a0", false);
	const LifecyclePlan active = BuildRestartCommandPlan(true, "c1a0", true);

	return stopped.action == LifecycleAction::NoOp &&
		active.action == LifecycleAction::LoadCurrentMap &&
		active.mapName == "c1a0" &&
		active.background;
}

static bool TestReloadCommand()
{
	return BuildReloadCommandPlan(false).action == LifecycleAction::NoOp &&
		BuildReloadCommandPlan(true).action == LifecycleAction::ReloadLatestSave;
}

static bool TestChangeLevelCommands()
{
	const LifecyclePlan missing =
		BuildChangeLevelCommandPlan(false, 1, nullptr, nullptr);
	const LifecyclePlan classic =
		BuildChangeLevelCommandPlan(false, 3, "c1a1.bsp", "ignored");
	const LifecyclePlan smoothNoLandmark =
		BuildChangeLevelCommandPlan(true, 2, "c1a2", nullptr);
	const LifecyclePlan smooth =
		BuildChangeLevelCommandPlan(true, 3, "c1a3", "lm1");

	return missing.action == LifecycleAction::PrintUsage &&
		classic.action == LifecycleAction::QueueChangeLevel &&
		classic.mapName == "c1a1.bsp" &&
		classic.landmarkName.empty() &&
		smoothNoLandmark.action == LifecycleAction::QueueChangeLevel &&
		smoothNoLandmark.landmarkName.empty() &&
		smooth.action == LifecycleAction::QueueChangeLevel &&
		smooth.mapName == "c1a3" &&
		smooth.landmarkName == "lm1";
}

static bool TestInfoCommandPrintsCurrentWithoutArguments()
{
	const OperatorInfoCommandPlan plan =
		BuildOperatorInfoCommandPlan(1, nullptr, nullptr);

	return plan.action == OperatorInfoCommandAction::PrintCurrent &&
		plan.key.empty() &&
		plan.value.empty();
}

static bool TestInfoCommandRejectsWrongArgumentCountsBeforeStarKeys()
{
	const OperatorInfoCommandPlan twoArgs =
		BuildOperatorInfoCommandPlan(2, "*protected", nullptr);
	const OperatorInfoCommandPlan fourArgs =
		BuildOperatorInfoCommandPlan(4, "hostname", "value");

	return twoArgs.action == OperatorInfoCommandAction::PrintUsage &&
		fourArgs.action == OperatorInfoCommandAction::PrintUsage;
}

static bool TestInfoCommandRejectsStarKeysOnlyWhenSetting()
{
	const OperatorInfoCommandPlan plan =
		BuildOperatorInfoCommandPlan(3, "*protected", "value");

	return plan.action == OperatorInfoCommandAction::RejectStarKey &&
		plan.key == "*protected" &&
		plan.value == "value";
}

static bool TestInfoCommandAllowsRegularKeyMutation()
{
	const OperatorInfoCommandPlan plan =
		BuildOperatorInfoCommandPlan(3, "hostname", "lambda");

	return plan.action == OperatorInfoCommandAction::SetValue &&
		plan.key == "hostname" &&
		plan.value == "lambda";
}

static bool TestKickCommandRequiresTargetArgument()
{
	return BuildOperatorKickCommandPlan(1, nullptr, nullptr).action ==
		OperatorKickCommandAction::PrintUsage;
}

static bool TestKickCommandClassifiesHashUserId()
{
	const OperatorKickCommandPlan plan =
		BuildOperatorKickCommandPlan(3, "#42", "reason");

	return plan.action == OperatorKickCommandAction::FindByUserId &&
		plan.userId == 42 &&
		plan.target == "#42" &&
		plan.reason == "reason";
}

static bool TestKickCommandRequiresAllDigitsAfterHash()
{
	const OperatorKickCommandPlan plan =
		BuildOperatorKickCommandPlan(2, "#12abc", nullptr);

	return plan.action == OperatorKickCommandAction::FindByName &&
		plan.target == "#12abc" &&
		plan.reason.empty();
}

static bool TestKickCommandKeepsNamesAndEmptyReason()
{
	const OperatorKickCommandPlan plan =
		BuildOperatorKickCommandPlan(2, "Gordon", nullptr);

	return plan.action == OperatorKickCommandAction::FindByName &&
		plan.target == "Gordon" &&
		plan.reason.empty();
}

}

int main()
{
	if (!TestMapCommandRequiresOneArgumentAndStripsExtension() ||
		!TestMapCommandKeepsDotsBeforePathSeparator() ||
		!TestBackgroundMapDedicatedTakesPrecedence() ||
		!TestBackgroundMapUsageAndActiveRejection() ||
		!TestBackgroundMapCanReplaceExistingBackground() ||
		!TestMapValidationClassification() ||
		!TestLoadCommandBuildsLegacyPath() ||
		!TestSaveCommandDefaultAndExplicitNames() ||
		!TestAutosaveCommand() ||
		!TestQuickAliases() ||
		!TestRestartCommand() ||
		!TestReloadCommand() ||
		!TestChangeLevelCommands() ||
		!TestInfoCommandPrintsCurrentWithoutArguments() ||
		!TestInfoCommandRejectsWrongArgumentCountsBeforeStarKeys() ||
		!TestInfoCommandRejectsStarKeysOnlyWhenSetting() ||
		!TestInfoCommandAllowsRegularKeyMutation() ||
		!TestKickCommandRequiresTargetArgument() ||
		!TestKickCommandClassifiesHashUserId() ||
		!TestKickCommandRequiresAllDigitsAfterHash() ||
		!TestKickCommandKeepsNamesAndEmptyReason())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
