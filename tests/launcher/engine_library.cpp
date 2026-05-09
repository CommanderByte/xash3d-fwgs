#include "launcher/engine_library.hpp"

using namespace xash::launcher;

static bool TestInitialState()
{
	EngineLibrary library;

	return !library.isLoaded() &&
		!library.hasHostMain() &&
		!library.hasHostShutdown();
}

static bool TestRunWithoutLoadFails()
{
	EngineLibrary library;
	LaunchSettings settings = MakeLaunchSettings("valve", false);

	return library.run(0, 0, settings, 0) == -1;
}

static bool TestUnloadIsIdempotent()
{
	EngineLibrary library;

	library.shutdownAndUnload();
	library.shutdownAndUnload();

	return !library.isLoaded() &&
		!library.hasHostMain() &&
		!library.hasHostShutdown();
}

int main()
{
	if (!TestInitialState())
		return 1;
	if (!TestRunWithoutLoadFails())
		return 2;
	if (!TestUnloadIsIdempotent())
		return 3;

	return 0;
}
