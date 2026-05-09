#include "launcher/launch_settings.hpp"

#include "port.h"

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

LaunchSettings MakeLaunchSettings(const char *defaultGameDir, bool disableMenuChangeGame)
{
	LaunchSettings settings;
	settings.defaultGameDir = defaultGameDir ? defaultGameDir : "";
	settings.allowMenuChangeGame = !disableMenuChangeGame;
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
#if XASH_WIN32
	return "xash.dll";
#else
	return OS_LIB_PREFIX "xash." OS_LIB_EXT;
#endif
}

const char *Sdl2LibraryName()
{
#if XASH_WIN32
	return "SDL2.dll";
#else
	return "";
#endif
}

bool ShouldProbeSdl2Library()
{
#if XASH_WIN32
	return true;
#else
	return false;
#endif
}

#if XASH_WIN32
const wchar_t *EngineLibraryNameWide()
{
	return L"xash.dll";
}

const wchar_t *Sdl2LibraryNameWide()
{
	return L"SDL2.dll";
}
#endif

}
}
