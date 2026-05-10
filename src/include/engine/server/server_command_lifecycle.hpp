#ifndef XASH_ENGINE_SERVER_SERVER_COMMAND_LIFECYCLE_HPP
#define XASH_ENGINE_SERVER_SERVER_COMMAND_LIFECYCLE_HPP

#include <string>

namespace xash
{
namespace engine
{
namespace server
{

constexpr unsigned int kLifecycleMapExists = 1u << 0;
constexpr unsigned int kLifecycleMapInvalidVersion = 1u << 3;
constexpr const char *kLifecycleSaveDirectory = "save/";
constexpr const char *kLifecycleSaveExtension = ".sav";
constexpr const char *kQuickLoadCommandText = "echo Quick Loading...; wait; load quick\n";
constexpr const char *kQuickSaveCommandText = "echo Quick Saving...; wait; save quick\n";

enum class LifecycleAction
{
	NoOp = 0,
	PrintUsage = 1,
	ValidateMap = 2,
	LoadGame = 3,
	SaveGame = 4,
	AddCommandText = 5,
	LoadCurrentMap = 6,
	ReloadLatestSave = 7,
	QueueChangeLevel = 8,
	BackgroundMapDedicatedError = 9,
	BackgroundMapActiveError = 10
};

enum class MapLaunchMode
{
	Foreground,
	Background
};

enum class MapValidationResult
{
	Valid = 0,
	InvalidVersion = 1,
	Missing = 2
};

struct BackgroundMapContext
{
	bool dedicated;
	bool serverActive;
	bool alreadyBackground;
	bool nextStateRunFrame;
};

struct LifecyclePlan
{
	LifecycleAction action;
	std::string mapName;
	std::string landmarkName;
	std::string saveName;
	std::string savePath;
	std::string commandText;
	bool background;
};

LifecyclePlan BuildMapCommandPlan(
	int argumentCount,
	const char *mapArgument,
	MapLaunchMode mode,
	const BackgroundMapContext &backgroundContext);
MapValidationResult ClassifyMapValidation(unsigned int mapFlags);

LifecyclePlan BuildLoadCommandPlan(int argumentCount, const char *saveArgument);
LifecyclePlan BuildSaveCommandPlan(int argumentCount, const char *saveArgument);
LifecyclePlan BuildAutosaveCommandPlan(int argumentCount, bool autosaveEnabled);
LifecyclePlan BuildQuickLoadCommandPlan();
LifecyclePlan BuildQuickSaveCommandPlan();
LifecyclePlan BuildRestartCommandPlan(
	bool serverActive,
	const char *currentMap,
	bool background);
LifecyclePlan BuildReloadCommandPlan(bool nextStateRunFrame);
LifecyclePlan BuildChangeLevelCommandPlan(
	bool smoothCommand,
	int argumentCount,
	const char *mapArgument,
	const char *landmarkArgument);

}
}
}

#endif
