#include <stdlib.h>

#include "engine/console/platform_console_backend.hpp"

using namespace xash::engine::console;

static bool TestCapabilityMasks()
{
	const PlatformConsoleCapabilities output =
		PlatformConsoleCapabilityMask(PlatformConsoleCapability::Output);
	const PlatformConsoleCapabilities input =
		PlatformConsoleCapabilityMask(PlatformConsoleCapability::Input);
	const PlatformConsoleCapabilities status =
		PlatformConsoleCapabilityMask(PlatformConsoleCapability::StatusLine);
	const PlatformConsoleCapabilities combined = output | input;

	return output != 0 &&
		input != 0 &&
		status != 0 &&
		PlatformConsoleHasCapability(combined, PlatformConsoleCapability::Output) &&
		PlatformConsoleHasCapability(combined, PlatformConsoleCapability::Input) &&
		!PlatformConsoleHasCapability(combined, PlatformConsoleCapability::StatusLine);
}

static bool TestDefaultConfig()
{
	const PlatformConsoleConfig config;

	return !config.dedicated &&
		!config.showAlways &&
		config.developerLevel == 0;
}

static bool TestNullBackendCapabilities()
{
	NullPlatformConsoleBackend backend;

	return backend.capabilities() == 0 &&
		!PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::Output) &&
		!PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::Input) &&
		backend.readCommand() == nullptr &&
		!backend.initialized();
}

static bool TestNullBackendLifecycle()
{
	NullPlatformConsoleBackend backend;
	PlatformConsoleConfig config;

	config.dedicated = true;
	config.showAlways = true;
	config.developerLevel = 2;

	backend.initialize(config);

	if (!backend.initialized())
		return false;

	if (!backend.lastConfig().dedicated ||
		!backend.lastConfig().showAlways ||
		backend.lastConfig().developerLevel != 2)
	{
		return false;
	}

	backend.print("ignored");
	backend.print(nullptr);
	backend.show(true);
	backend.show(false);
	backend.disableInput();
	backend.setStatus("ignored");
	backend.setStatus(nullptr);
	backend.registerCommands();

	if (backend.readCommand() != nullptr)
		return false;

	backend.shutdown();
	return !backend.initialized();
}

int main()
{
	if (!TestCapabilityMasks() ||
		!TestDefaultConfig() ||
		!TestNullBackendCapabilities() ||
		!TestNullBackendLifecycle())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
