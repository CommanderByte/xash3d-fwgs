#include <string.h>

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
		ExpectString(enabled.engineLibraryName, EngineLibraryName()) &&
		ExpectString(enabled.sdl2LibraryName, Sdl2LibraryName()) &&
		enabled.probeSdl2Library == ShouldProbeSdl2Library() &&
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

	if (ShouldProbeSdl2Library())
	{
		return ExpectString(Sdl2LibraryName(), "SDL2.dll");
	}

	return ExpectString(Sdl2LibraryName(), "");
}

static bool TestJsonOverrides()
{
	LaunchSettings settings = GetDefaultLaunchSettings();
	const char *json =
		"{"
		"\"defaultGameDir\":\"gearbox\","
		"\"allowMenuChangeGame\":false,"
		"\"engineLibrary\":\"custom_xash.dll\","
		"\"sdl2Library\":\"custom_sdl2.dll\","
		"\"probeSdl2\":false"
		"}";

	if (!ApplyLaunchSettingsJson(json, &settings))
	{
		return false;
	}

	return ExpectString(settings.defaultGameDir, "gearbox") &&
		!settings.allowMenuChangeGame &&
		ExpectString(settings.engineLibraryName, "custom_xash.dll") &&
		ExpectString(settings.sdl2LibraryName, "custom_sdl2.dll") &&
		!settings.probeSdl2Library;
}

static bool TestJsonSnakeCaseAndDisableAlias()
{
	LaunchSettings settings = GetDefaultLaunchSettings();
	const char *json =
		"{"
		"\"default_game_dir\":\"bshift\","
		"\"disable_menu_change_game\":true,"
		"\"engine_library\":\"libcustom.so\","
		"\"sdl2_library\":\"\","
		"\"probe_sdl2\":false"
		"}";

	if (!ApplyLaunchSettingsJson(json, &settings))
	{
		return false;
	}

	return ExpectString(settings.defaultGameDir, "bshift") &&
		!settings.allowMenuChangeGame &&
		ExpectString(settings.engineLibraryName, "libcustom.so") &&
		ExpectString(settings.sdl2LibraryName, "") &&
		!settings.probeSdl2Library;
}

static bool TestInvalidJsonKeepsDefaults()
{
	LaunchSettings settings = GetDefaultLaunchSettings();
	LaunchSettings before = settings;

	return !ApplyLaunchSettingsJson("{\"defaultGameDir\":true}", &settings) &&
		ExpectString(settings.defaultGameDir, before.defaultGameDir) &&
		settings.allowMenuChangeGame == before.allowMenuChangeGame &&
		ExpectString(settings.engineLibraryName, before.engineLibraryName);
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
	if (!TestJsonOverrides())
		return 6;
	if (!TestJsonSnakeCaseAndDisableAlias())
		return 7;
	if (!TestInvalidJsonKeepsDefaults())
		return 8;

	return 0;
}
