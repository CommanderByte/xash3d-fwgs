#ifndef XASH_LAUNCHER_LAUNCH_SETTINGS_HPP
#define XASH_LAUNCHER_LAUNCH_SETTINGS_HPP

#include "port.h"

namespace xash
{
namespace launcher
{

enum
{
	LauncherStringMax = 256
};

struct LaunchSettings
{
	char defaultGameDir[LauncherStringMax];
	bool allowMenuChangeGame;
	char engineLibraryName[LauncherStringMax];
	char sdl2LibraryName[LauncherStringMax];
	bool probeSdl2Library;
};

struct EngineExportNames
{
	const char *hostMain;
	const char *hostShutdown;
};

LaunchSettings MakeLaunchSettings(const char *defaultGameDir, bool disableMenuChangeGame);
LaunchSettings GetDefaultLaunchSettings();
LaunchSettings GetLaunchSettings(int argc, char **argv);
bool ApplyLaunchSettingsJson(const char *json, LaunchSettings *settings);
bool LoadLaunchSettingsFile(const char *path, LaunchSettings *settings);
const char *LauncherConfigFileName();

EngineExportNames GetEngineExportNames();

const char *EngineLibraryName();
const char *Sdl2LibraryName();
bool ShouldProbeSdl2Library();

}
}

#endif
