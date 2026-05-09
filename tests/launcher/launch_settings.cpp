#include <string.h>

#if XASH_WIN32
#include <wchar.h>
#endif

#include "launcher/launch_settings.hpp"

#ifndef XASH_GAMEDIR
#define XASH_GAMEDIR "valve"
#endif

#ifndef XASH_DISABLE_MENU_CHANGEGAME
#define XASH_DISABLE_MENU_CHANGEGAME 0
#endif

using namespace xash::launcher;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static bool TestMenuChangeFlag()
{
	LaunchSettings enabled = MakeLaunchSettings("valve", false);
	LaunchSettings disabled = MakeLaunchSettings("valve", true);

	return ExpectString(enabled.defaultGameDir, "valve") &&
		enabled.allowMenuChangeGame &&
		ExpectString(disabled.defaultGameDir, "valve") &&
		!disabled.allowMenuChangeGame;
}

static bool TestNullGameDirIsSafe()
{
	LaunchSettings settings = MakeLaunchSettings(0, false);

	return ExpectString(settings.defaultGameDir, "") &&
		settings.allowMenuChangeGame;
}

static bool TestDefaultSettings()
{
	LaunchSettings settings = GetDefaultLaunchSettings();

	if (!ExpectString(settings.defaultGameDir, XASH_GAMEDIR))
	{
		return false;
	}

	return settings.allowMenuChangeGame == !XASH_DISABLE_MENU_CHANGEGAME;
}

static bool TestEngineExports()
{
	EngineExportNames names = GetEngineExportNames();

	return ExpectString(names.hostMain, "Host_Main") &&
		ExpectString(names.hostShutdown, "Host_Shutdown");
}

static bool TestLibraryNames()
{
	if (!ExpectString(EngineLibraryName(),
#if XASH_WIN32
		"xash.dll"
#else
		OS_LIB_PREFIX "xash." OS_LIB_EXT
#endif
	))
	{
		return false;
	}

#if XASH_WIN32
	if (!ShouldProbeSdl2Library() ||
		!ExpectString(Sdl2LibraryName(), "SDL2.dll") ||
		wcscmp(EngineLibraryNameWide(), L"xash.dll") != 0 ||
		wcscmp(Sdl2LibraryNameWide(), L"SDL2.dll") != 0)
	{
		return false;
	}
#else
	if (ShouldProbeSdl2Library() ||
		!ExpectString(Sdl2LibraryName(), ""))
	{
		return false;
	}
#endif

	return true;
}

int main()
{
	if (!TestMenuChangeFlag())
		return 1;
	if (!TestNullGameDirIsSafe())
		return 2;
	if (!TestDefaultSettings())
		return 3;
	if (!TestEngineExports())
		return 4;
	if (!TestLibraryNames())
		return 5;

	return 0;
}
