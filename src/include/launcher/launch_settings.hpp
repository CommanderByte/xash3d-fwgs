#ifndef XASH_LAUNCHER_LAUNCH_SETTINGS_HPP
#define XASH_LAUNCHER_LAUNCH_SETTINGS_HPP

#include "port.h"

namespace xash
{
namespace launcher
{

struct LaunchSettings
{
	const char *defaultGameDir;
	bool allowMenuChangeGame;
};

struct EngineExportNames
{
	const char *hostMain;
	const char *hostShutdown;
};

LaunchSettings MakeLaunchSettings(const char *defaultGameDir, bool disableMenuChangeGame);
LaunchSettings GetDefaultLaunchSettings();

EngineExportNames GetEngineExportNames();

const char *EngineLibraryName();
const char *Sdl2LibraryName();
bool ShouldProbeSdl2Library();

#if XASH_WIN32
const wchar_t *EngineLibraryNameWide();
const wchar_t *Sdl2LibraryNameWide();
#endif

}
}

#endif
