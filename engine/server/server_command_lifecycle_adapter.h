#ifndef XASH_ENGINE_SERVER_COMMAND_LIFECYCLE_ADAPTER_H
#define XASH_ENGINE_SERVER_COMMAND_LIFECYCLE_ADAPTER_H

#include "xash3d_types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum sv_lifecycle_action_e
{
	SV_LIFECYCLE_ACTION_NOOP = 0,
	SV_LIFECYCLE_ACTION_PRINT_USAGE = 1,
	SV_LIFECYCLE_ACTION_VALIDATE_MAP = 2,
	SV_LIFECYCLE_ACTION_LOAD_GAME = 3,
	SV_LIFECYCLE_ACTION_SAVE_GAME = 4,
	SV_LIFECYCLE_ACTION_ADD_COMMAND_TEXT = 5,
	SV_LIFECYCLE_ACTION_LOAD_CURRENT_MAP = 6,
	SV_LIFECYCLE_ACTION_RELOAD_LATEST_SAVE = 7,
	SV_LIFECYCLE_ACTION_QUEUE_CHANGELEVEL = 8,
	SV_LIFECYCLE_ACTION_BACKGROUND_DEDICATED_ERROR = 9,
	SV_LIFECYCLE_ACTION_BACKGROUND_ACTIVE_ERROR = 10
};

enum sv_lifecycle_map_validation_e
{
	SV_LIFECYCLE_MAP_VALID = 0,
	SV_LIFECYCLE_MAP_INVALID_VERSION = 1,
	SV_LIFECYCLE_MAP_MISSING = 2
};

typedef struct sv_lifecycle_plan_s
{
	enum sv_lifecycle_action_e action;
	char map_name[MAX_QPATH];
	char landmark_name[MAX_STRING];
	char save_name[MAX_STRING];
	char save_path[MAX_QPATH];
	char command_text[MAX_STRING];
	int background;
} sv_lifecycle_plan_t;

sv_lifecycle_plan_t SV_Lifecycle_BuildMapCommandPlan(
	int argument_count,
	const char *map_argument);
sv_lifecycle_plan_t SV_Lifecycle_BuildBackgroundMapCommandPlan(
	int argument_count,
	const char *map_argument,
	int dedicated,
	int server_active,
	int already_background,
	int next_state_runframe);
enum sv_lifecycle_map_validation_e SV_Lifecycle_ClassifyMapValidation(
	unsigned int map_flags);
sv_lifecycle_plan_t SV_Lifecycle_BuildLoadCommandPlan(
	int argument_count,
	const char *save_argument);
sv_lifecycle_plan_t SV_Lifecycle_BuildSaveCommandPlan(
	int argument_count,
	const char *save_argument);
sv_lifecycle_plan_t SV_Lifecycle_BuildAutosaveCommandPlan(
	int argument_count,
	int autosave_enabled);
sv_lifecycle_plan_t SV_Lifecycle_BuildQuickLoadCommandPlan(void);
sv_lifecycle_plan_t SV_Lifecycle_BuildQuickSaveCommandPlan(void);
sv_lifecycle_plan_t SV_Lifecycle_BuildRestartCommandPlan(
	int server_active,
	const char *current_map,
	int background);
sv_lifecycle_plan_t SV_Lifecycle_BuildReloadCommandPlan(int next_state_runframe);
sv_lifecycle_plan_t SV_Lifecycle_BuildChangeLevelCommandPlan(
	int smooth_command,
	int argument_count,
	const char *map_argument,
	const char *landmark_argument);

#ifdef __cplusplus
}
#endif

#endif
