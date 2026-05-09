#include <string.h>

#include "launcher/application.hpp"

#ifndef XASH_GAMEDIR
#define XASH_GAMEDIR "valve"
#endif

#ifndef XASH_DISABLE_MENU_CHANGEGAME
#define XASH_DISABLE_MENU_CHANGEGAME 0
#endif

using namespace xash::launcher;

static int g_shutdownCalls;

static int FakeHostMain(int argc, char **argv, const char *progname, int changeGame, ChangeGameFn func)
{
	if (argc != 2 || !argv || strcmp(argv[0], "xash3d") != 0 || strcmp(argv[1], "-dev") != 0)
		return 10;
	if (strcmp(progname, XASH_GAMEDIR) != 0)
		return 11;
	if (changeGame != 0)
		return 12;
	if ((func != 0) != !XASH_DISABLE_MENU_CHANGEGAME)
		return 13;

	return 42;
}

static void FakeHostShutdown()
{
	++g_shutdownCalls;
}

static void FakeChangeGame(const char *)
{
}

static bool TestRunApplicationSuccess()
{
	EngineLibrary engineLibrary;
	char errorBuffer[128] = "unchanged";
	char arg0[] = "xash3d";
	char arg1[] = "-dev";
	char *argv[] = { arg0, arg1, 0 };

	engineLibrary.setLoadedForTest((void *)1, FakeHostMain, FakeHostShutdown);
	g_shutdownCalls = 0;

	int result = RunApplication(2, argv, engineLibrary, FakeChangeGame, errorBuffer, sizeof(errorBuffer));

	return result == 42 &&
		g_shutdownCalls == 1 &&
		!engineLibrary.isLoaded() &&
		errorBuffer[0] == '\0';
}

static bool TestRunApplicationLoadFailure()
{
	EngineLibrary engineLibrary;
	char errorBuffer[128];
	char *argv[] = { 0 };

	int result = RunApplication(0, argv, engineLibrary, 0, errorBuffer, sizeof(errorBuffer));

	return result == -1 &&
		!engineLibrary.isLoaded() &&
		errorBuffer[0] != '\0';
}

int main()
{
	if (!TestRunApplicationSuccess())
		return 1;
	if (!TestRunApplicationLoadFailure())
		return 2;

	return 0;
}
