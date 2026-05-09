#include <stdlib.h>

#include <string>

#include "engine/console/platform_console_backend.hpp"

using namespace xash::engine::console;

class FakePosixIo : public IPosixConsoleIo
{
public:
	FakePosixIo()
		: output()
		, readCount(0)
		, command(nullptr)
	{
	}

	void writeOutput(const char *text) override
	{
		if (text)
			output += text;
	}

	const char *readCommand() override
	{
		++readCount;
		return command;
	}

	std::string output;
	int readCount;
	const char *command;
};

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

static bool TestPosixBackendCapabilities()
{
	FakePosixIo io;
	PosixPlatformConsoleBackend backend(io);

	return PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::Output) &&
		PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::Input) &&
		!PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::Visibility) &&
		!PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::StatusLine) &&
		!PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::CommandRegistration) &&
		!backend.initialized() &&
		!backend.inputEnabled();
}

static bool TestPosixBackendOutputLifecycle()
{
	FakePosixIo io;
	PosixPlatformConsoleBackend backend(io);
	PlatformConsoleConfig config;

	backend.print("before-init");
	if (!io.output.empty())
		return false;

	backend.initialize(config);
	backend.print("hello");
	backend.print(nullptr);
	backend.print(" world");

	if (io.output != "hello world")
		return false;

	backend.shutdown();
	backend.print("after-shutdown");
	return io.output == "hello world";
}

static bool TestPosixBackendNoOutputCapability()
{
	FakePosixIo io;
	PosixPlatformConsoleBackend backend(io,
		PlatformConsoleCapabilityMask(PlatformConsoleCapability::Input));
	PlatformConsoleConfig config;

	backend.initialize(config);
	backend.print("ignored");

	return io.output.empty();
}

static bool TestPosixBackendInputRequiresDedicated()
{
	FakePosixIo io;
	PosixPlatformConsoleBackend backend(io);
	PlatformConsoleConfig config;

	io.command = "status\n";
	backend.initialize(config);

	if (backend.readCommand() != nullptr || io.readCount != 0)
		return false;

	config.dedicated = true;
	backend.initialize(config);

	if (backend.readCommand() != io.command || io.readCount != 1)
		return false;

	backend.disableInput();
	if (backend.inputEnabled())
		return false;

	return backend.readCommand() == nullptr && io.readCount == 1;
}

static bool TestPosixBackendNoInputCapability()
{
	FakePosixIo io;
	PosixPlatformConsoleBackend backend(io,
		PlatformConsoleCapabilityMask(PlatformConsoleCapability::Output));
	PlatformConsoleConfig config;

	config.dedicated = true;
	io.command = "quit\n";
	backend.initialize(config);

	return backend.readCommand() == nullptr && io.readCount == 0;
}

int main()
{
	if (!TestCapabilityMasks() ||
		!TestDefaultConfig() ||
		!TestNullBackendCapabilities() ||
		!TestNullBackendLifecycle() ||
		!TestPosixBackendCapabilities() ||
		!TestPosixBackendOutputLifecycle() ||
		!TestPosixBackendNoOutputCapability() ||
		!TestPosixBackendInputRequiresDedicated() ||
		!TestPosixBackendNoInputCapability())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
