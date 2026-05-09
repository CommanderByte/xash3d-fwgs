#include "launcher/application.hpp"

#if XASH_SAILFISH
#include <stdio.h>
#include <stdlib.h>
#endif

namespace xash
{
namespace launcher
{

static void ApplyPlatformEnvironment()
{
#if XASH_SAILFISH
	const char *home = getenv("HOME");
	char buffer[1024];

	snprintf(buffer, sizeof(buffer), "%s/xash", home);
	setenv("XASH3D_BASEDIR", buffer, true);
	setenv("XASH3D_RODIR", "/usr/share/harbour-xash3d-fwgs/rodir", true);
#endif
}

int RunApplication(int argc, char **argv, EngineLibrary &engineLibrary, ChangeGameFn changeGame,
	char *errorBuffer, size_t errorBufferSize)
{
	LaunchSettings settings = GetDefaultLaunchSettings();

	ApplyPlatformEnvironment();

	if (errorBuffer && errorBufferSize)
	{
		errorBuffer[0] = '\0';
	}

	if (!engineLibrary.isLoaded() && !engineLibrary.load(errorBuffer, errorBufferSize))
	{
		return -1;
	}

	int result = engineLibrary.run(argc, argv, settings,
		settings.allowMenuChangeGame ? changeGame : 0);
	engineLibrary.shutdownAndUnload();
	return result;
}

}
}
