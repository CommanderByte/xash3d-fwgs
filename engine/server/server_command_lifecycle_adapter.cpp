#include "server_command_lifecycle_adapter.h"

#include "engine/server/runtime/server_command_lifecycle.hpp"

#include <cstring>
#include <string>

namespace
{

template <std::size_t Size>
void CopyText(char (&destination)[Size], const std::string &source)
{
	if (Size == 0)
		return;

	std::strncpy(destination, source.c_str(), Size - 1);
	destination[Size - 1] = '\0';
}

sv_lifecycle_plan_t ToLegacyPlan(
	const xash::engine::server::LifecyclePlan &plan)
{
	sv_lifecycle_plan_t legacy = {};
	legacy.action = static_cast<sv_lifecycle_action_e>(plan.action);
	CopyText(legacy.map_name, plan.mapName);
	CopyText(legacy.landmark_name, plan.landmarkName);
	CopyText(legacy.save_name, plan.saveName);
	CopyText(legacy.save_path, plan.savePath);
	CopyText(legacy.command_text, plan.commandText);
	legacy.background = plan.background ? 1 : 0;
	return legacy;
}

}

extern "C" sv_lifecycle_plan_t SV_Lifecycle_BuildMapCommandPlan(
	int argument_count,
	const char *map_argument)
{
	xash::engine::server::BackgroundMapContext context = {};
	return ToLegacyPlan(xash::engine::server::BuildMapCommandPlan(
		argument_count,
		map_argument,
		xash::engine::server::MapLaunchMode::Foreground,
		context));
}

extern "C" sv_lifecycle_plan_t SV_Lifecycle_BuildBackgroundMapCommandPlan(
	int argument_count,
	const char *map_argument,
	int dedicated,
	int server_active,
	int already_background,
	int next_state_runframe)
{
	xash::engine::server::BackgroundMapContext context = {};
	context.dedicated = dedicated != 0;
	context.serverActive = server_active != 0;
	context.alreadyBackground = already_background != 0;
	context.nextStateRunFrame = next_state_runframe != 0;

	return ToLegacyPlan(xash::engine::server::BuildMapCommandPlan(
		argument_count,
		map_argument,
		xash::engine::server::MapLaunchMode::Background,
		context));
}

extern "C" enum sv_lifecycle_map_validation_e SV_Lifecycle_ClassifyMapValidation(
	unsigned int map_flags)
{
	return static_cast<sv_lifecycle_map_validation_e>(
		xash::engine::server::ClassifyMapValidation(map_flags));
}

extern "C" sv_lifecycle_plan_t SV_Lifecycle_BuildLoadCommandPlan(
	int argument_count,
	const char *save_argument)
{
	return ToLegacyPlan(xash::engine::server::BuildLoadCommandPlan(
		argument_count,
		save_argument));
}

extern "C" sv_lifecycle_plan_t SV_Lifecycle_BuildSaveCommandPlan(
	int argument_count,
	const char *save_argument)
{
	return ToLegacyPlan(xash::engine::server::BuildSaveCommandPlan(
		argument_count,
		save_argument));
}

extern "C" sv_lifecycle_plan_t SV_Lifecycle_BuildAutosaveCommandPlan(
	int argument_count,
	int autosave_enabled)
{
	return ToLegacyPlan(xash::engine::server::BuildAutosaveCommandPlan(
		argument_count,
		autosave_enabled != 0));
}

extern "C" sv_lifecycle_plan_t SV_Lifecycle_BuildQuickLoadCommandPlan(void)
{
	return ToLegacyPlan(xash::engine::server::BuildQuickLoadCommandPlan());
}

extern "C" sv_lifecycle_plan_t SV_Lifecycle_BuildQuickSaveCommandPlan(void)
{
	return ToLegacyPlan(xash::engine::server::BuildQuickSaveCommandPlan());
}

extern "C" sv_lifecycle_plan_t SV_Lifecycle_BuildRestartCommandPlan(
	int server_active,
	const char *current_map,
	int background)
{
	return ToLegacyPlan(xash::engine::server::BuildRestartCommandPlan(
		server_active != 0,
		current_map,
		background != 0));
}

extern "C" sv_lifecycle_plan_t SV_Lifecycle_BuildReloadCommandPlan(
	int next_state_runframe)
{
	return ToLegacyPlan(xash::engine::server::BuildReloadCommandPlan(
		next_state_runframe != 0));
}

extern "C" sv_lifecycle_plan_t SV_Lifecycle_BuildChangeLevelCommandPlan(
	int smooth_command,
	int argument_count,
	const char *map_argument,
	const char *landmark_argument)
{
	return ToLegacyPlan(xash::engine::server::BuildChangeLevelCommandPlan(
		smooth_command != 0,
		argument_count,
		map_argument,
		landmark_argument));
}
