#include "engine/console/platform_console_backend_adapter.h"

#include "engine/console/platform_console_backend.hpp"

#if XASH_WIN32

extern "C" {
void Wcon_LegacyCreateConsole(int showAlways);
void Wcon_LegacyDestroyConsole(void);
void Wcon_LegacyWinPrint(const char *text);
char *Wcon_LegacyInput(void);
void Wcon_LegacyShowConsole(int visible);
void Wcon_LegacyDisableInput(void);
void Wcon_LegacySetStatus(const char *text);
void Wcon_LegacyRegisterCommands(void);
}

namespace
{

class LiveWin32ConsoleIo : public xash::engine::console::IWin32ConsoleIo
{
public:
	void initialize(const xash::engine::console::PlatformConsoleConfig &config) override
	{
		Wcon_LegacyCreateConsole(config.showAlways ? 1 : 0);
	}

	void shutdown() override
	{
		Wcon_LegacyDestroyConsole();
	}

	void print(const char *text) override
	{
		Wcon_LegacyWinPrint(text);
	}

	const char *readCommand() override
	{
		return Wcon_LegacyInput();
	}

	void show(bool visible) override
	{
		Wcon_LegacyShowConsole(visible ? 1 : 0);
	}

	void disableInput() override
	{
		Wcon_LegacyDisableInput();
	}

	void setStatus(const char *text) override
	{
		Wcon_LegacySetStatus(text);
	}

	void registerCommands() override
	{
		Wcon_LegacyRegisterCommands();
	}
};

LiveWin32ConsoleIo &ConsoleIo()
{
	static LiveWin32ConsoleIo io;
	return io;
}

xash::engine::console::Win32PlatformConsoleBackend &ConsoleBackend()
{
	static xash::engine::console::Win32PlatformConsoleBackend backend(ConsoleIo());
	return backend;
}

}

#endif

extern "C" void Xash_Win32Console_Create(int dedicated, int showAlways, int developerLevel)
{
#if XASH_WIN32
	xash::engine::console::PlatformConsoleConfig config;
	config.dedicated = dedicated != 0;
	config.showAlways = showAlways != 0;
	config.developerLevel = developerLevel;
	ConsoleBackend().initialize(config);
#else
	(void)dedicated;
	(void)showAlways;
	(void)developerLevel;
#endif
}

extern "C" void Xash_Win32Console_Destroy(void)
{
#if XASH_WIN32
	ConsoleBackend().shutdown();
#endif
}

extern "C" void Xash_Win32Console_Print(const char *text)
{
#if XASH_WIN32
	ConsoleBackend().print(text);
#else
	(void)text;
#endif
}

extern "C" char *Xash_Win32Console_Input(void)
{
#if XASH_WIN32
	return const_cast<char *>(ConsoleBackend().readCommand());
#else
	return nullptr;
#endif
}

extern "C" void Xash_Win32Console_Show(int visible)
{
#if XASH_WIN32
	ConsoleBackend().show(visible != 0);
#else
	(void)visible;
#endif
}

extern "C" void Xash_Win32Console_DisableInput(void)
{
#if XASH_WIN32
	ConsoleBackend().disableInput();
#endif
}

extern "C" void Xash_Win32Console_SetStatus(const char *text)
{
#if XASH_WIN32
	ConsoleBackend().setStatus(text);
#else
	(void)text;
#endif
}

extern "C" void Xash_Win32Console_RegisterCommands(void)
{
#if XASH_WIN32
	ConsoleBackend().registerCommands();
#endif
}
