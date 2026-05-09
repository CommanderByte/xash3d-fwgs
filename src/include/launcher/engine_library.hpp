#ifndef XASH_LAUNCHER_ENGINE_LIBRARY_HPP
#define XASH_LAUNCHER_ENGINE_LIBRARY_HPP

#include <stddef.h>

#include "launcher/launch_settings.hpp"

namespace xash
{
namespace launcher
{

typedef void (*ChangeGameFn)(const char *progname);
typedef int (*HostMainFn)(int argc, char **argv, const char *progname, int changeGame, ChangeGameFn func);
typedef void (*HostShutdownFn)(void);

class EngineLibrary
{
public:
	EngineLibrary();

	bool load(const LaunchSettings &settings, char *errorBuffer, size_t errorBufferSize);
	int run(int argc, char **argv, const LaunchSettings &settings, ChangeGameFn changeGame);
	void shutdownAndUnload();

	bool isLoaded() const;
	bool hasHostMain() const;
	bool hasHostShutdown() const;

	void setLoadedForTest(void *handle, HostMainFn hostMain, HostShutdownFn hostShutdown);

private:
	void clear();
	void formatError(char *errorBuffer, size_t errorBufferSize, const char *format, ...) const;

	void *m_handle;
	HostMainFn m_hostMain;
	HostShutdownFn m_hostShutdown;
	bool m_ownsHandle;
};

}
}

#endif
