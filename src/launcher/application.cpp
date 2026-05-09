#include "launcher/application.hpp"

#include "launcher/platform/environment.hpp"

namespace xash
{
namespace launcher
{

int RunApplication(int argc, char **argv, EngineLibrary &engineLibrary, ChangeGameFn changeGame,
	char *errorBuffer, size_t errorBufferSize)
{
	LaunchSettings settings = GetLaunchSettings(argc, argv);

	platform::ApplyEnvironmentDefaults();

	if (errorBuffer && errorBufferSize)
	{
		errorBuffer[0] = '\0';
	}

	if (!engineLibrary.isLoaded() && !engineLibrary.load(settings, errorBuffer, errorBufferSize))
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
