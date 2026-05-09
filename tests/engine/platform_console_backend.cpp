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

class FakeWin32Io : public IWin32ConsoleIo
{
public:
	FakeWin32Io()
		: output()
		, status()
		, command(nullptr)
		, initializeCount(0)
		, shutdownCount(0)
		, printCount(0)
		, readCount(0)
		, showCount(0)
		, disableInputCount(0)
		, setStatusCount(0)
		, registerCommandsCount(0)
		, lastShow(false)
		, lastConfig()
	{
	}

	void initialize(const PlatformConsoleConfig &config) override
	{
		lastConfig = config;
		++initializeCount;
	}

	void shutdown() override
	{
		++shutdownCount;
	}

	void print(const char *text) override
	{
		++printCount;
		if (text)
			output += text;
	}

	const char *readCommand() override
	{
		++readCount;
		return command;
	}

	void show(bool visible) override
	{
		lastShow = visible;
		++showCount;
	}

	void disableInput() override
	{
		++disableInputCount;
	}

	void setStatus(const char *text) override
	{
		++setStatusCount;
		if (text)
			status = text;
	}

	void registerCommands() override
	{
		++registerCommandsCount;
	}

	std::string output;
	std::string status;
	const char *command;
	int initializeCount;
	int shutdownCount;
	int printCount;
	int readCount;
	int showCount;
	int disableInputCount;
	int setStatusCount;
	int registerCommandsCount;
	bool lastShow;
	PlatformConsoleConfig lastConfig;
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

static bool TestWin32BackendCapabilities()
{
	FakeWin32Io io;
	Win32PlatformConsoleBackend backend(io);

	return PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::Output) &&
		PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::Input) &&
		PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::Visibility) &&
		PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::StatusLine) &&
		PlatformConsoleHasCapability(backend.capabilities(), PlatformConsoleCapability::CommandRegistration) &&
		!backend.initialized() &&
		!backend.inputEnabled();
}

static bool TestWin32BackendLifecycle()
{
	FakeWin32Io io;
	Win32PlatformConsoleBackend backend(io);
	PlatformConsoleConfig config;

	config.showAlways = true;
	config.developerLevel = 2;

	backend.print("before-init");
	if (io.printCount != 0)
		return false;

	backend.initialize(config);
	if (!backend.initialized() ||
		!backend.inputEnabled() ||
		io.initializeCount != 1 ||
		!io.lastConfig.showAlways ||
		io.lastConfig.developerLevel != 2)
	{
		return false;
	}

	backend.print("hello");
	backend.print(nullptr);
	backend.print(" console");
	backend.show(true);
	backend.setStatus("running");

	if (io.output != "hello console" ||
		io.printCount != 2 ||
		io.showCount != 1 ||
		!io.lastShow ||
		io.status != "running" ||
		io.setStatusCount != 1)
	{
		return false;
	}

	backend.shutdown();
	backend.shutdown();

	return !backend.initialized() &&
		!backend.inputEnabled() &&
		io.shutdownCount == 1;
}

static bool TestWin32BackendInputAndDedicatedControls()
{
	FakeWin32Io io;
	Win32PlatformConsoleBackend backend(io);
	PlatformConsoleConfig config;

	io.command = "status\n";
	backend.initialize(config);

	if (backend.readCommand() != io.command || io.readCount != 1)
		return false;

	backend.disableInput();
	backend.registerCommands();
	if (!backend.inputEnabled() ||
		io.disableInputCount != 0 ||
		io.registerCommandsCount != 0)
	{
		return false;
	}

	config.dedicated = true;
	backend.initialize(config);
	backend.disableInput();
	backend.registerCommands();

	if (backend.inputEnabled() ||
		io.disableInputCount != 1 ||
		io.registerCommandsCount != 1)
	{
		return false;
	}

	return backend.readCommand() == nullptr && io.readCount == 1;
}

static bool TestWin32BackendHonorsMissingCapabilities()
{
	FakeWin32Io io;
	Win32PlatformConsoleBackend backend(io, 0);
	PlatformConsoleConfig config;

	config.dedicated = true;
	io.command = "quit\n";
	backend.initialize(config);
	backend.print("ignored");
	backend.show(true);
	backend.setStatus("ignored");
	backend.disableInput();
	backend.registerCommands();

	return backend.readCommand() == nullptr &&
		io.readCount == 0 &&
		io.printCount == 0 &&
		io.showCount == 0 &&
		io.setStatusCount == 0 &&
		io.disableInputCount == 0 &&
		io.registerCommandsCount == 0 &&
		backend.inputEnabled();
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
		!TestPosixBackendNoInputCapability() ||
		!TestWin32BackendCapabilities() ||
		!TestWin32BackendLifecycle() ||
		!TestWin32BackendInputAndDedicatedControls() ||
		!TestWin32BackendHonorsMissingCapabilities())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
