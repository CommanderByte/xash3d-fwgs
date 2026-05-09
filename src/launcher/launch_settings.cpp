#include "launcher/launch_settings.hpp"

#include "launcher/platform/library.hpp"

#include "port.h"

#include <stdio.h>

#ifndef XASH_GAMEDIR
#define XASH_GAMEDIR "valve"
#endif

#ifndef XASH_DISABLE_MENU_CHANGEGAME
#define XASH_DISABLE_MENU_CHANGEGAME 0
#endif

namespace xash
{
namespace launcher
{

static void CopySettingString(char *dst, size_t dstSize, const char *src)
{
	if (!dst || !dstSize)
	{
		return;
	}

	if (!src)
	{
		src = "";
	}

	snprintf(dst, dstSize, "%s", src);
	dst[dstSize - 1] = '\0';
}

LaunchSettings MakeLaunchSettings(const char *defaultGameDir, bool disableMenuChangeGame)
{
	LaunchSettings settings;
	CopySettingString(settings.defaultGameDir, sizeof(settings.defaultGameDir), defaultGameDir);
	settings.allowMenuChangeGame = !disableMenuChangeGame;
	CopySettingString(settings.engineLibraryName, sizeof(settings.engineLibraryName),
		platform::DefaultEngineLibraryName());
	CopySettingString(settings.sdl2LibraryName, sizeof(settings.sdl2LibraryName),
		platform::DefaultSdl2LibraryName());
	settings.probeSdl2Library = platform::DefaultProbeSdl2Library();
	return settings;
}

LaunchSettings GetDefaultLaunchSettings()
{
	return MakeLaunchSettings(XASH_GAMEDIR, XASH_DISABLE_MENU_CHANGEGAME);
}

EngineExportNames GetEngineExportNames()
{
	EngineExportNames names;
	names.hostMain = "Host_Main";
	names.hostShutdown = "Host_Shutdown";
	return names;
}

const char *EngineLibraryName()
{
	return platform::DefaultEngineLibraryName();
}

const char *Sdl2LibraryName()
{
	return platform::DefaultSdl2LibraryName();
}

bool ShouldProbeSdl2Library()
{
	return platform::DefaultProbeSdl2Library();
}

}
}
