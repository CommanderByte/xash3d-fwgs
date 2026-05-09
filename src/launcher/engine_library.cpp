#include "launcher/engine_library.hpp"

#include "launcher/platform/library.hpp"

#include <stdarg.h>
#include <stdio.h>

namespace xash
{
namespace launcher
{

EngineLibrary::EngineLibrary()
{
	clear();
}

bool EngineLibrary::load(const LaunchSettings &settings, char *errorBuffer, size_t errorBufferSize)
{
	if (errorBuffer && errorBufferSize)
	{
		errorBuffer[0] = '\0';
	}

	EngineExportNames exports = GetEngineExportNames();

	if (settings.probeSdl2Library && settings.sdl2LibraryName[0] &&
		!platform::ProbeLibrary(settings.sdl2LibraryName, errorBuffer, errorBufferSize))
	{
		return false;
	}

	m_handle = platform::OpenLibrary(settings.engineLibraryName, errorBuffer, errorBufferSize);

	if (!m_handle)
	{
		return false;
	}

	m_ownsHandle = true;
	m_hostMain = (HostMainFn)platform::FindLibrarySymbol(m_handle, exports.hostMain);

	if (!m_hostMain)
	{
		formatError(errorBuffer, errorBufferSize, "%s missed '%s' export: %s",
			settings.engineLibraryName, exports.hostMain, platform::LastLibraryError());
		shutdownAndUnload();
		return false;
	}

	m_hostShutdown = (HostShutdownFn)platform::FindLibrarySymbol(m_handle, exports.hostShutdown);

	if (errorBuffer && errorBufferSize)
	{
		errorBuffer[0] = '\0';
	}

	return true;
}

int EngineLibrary::run(int argc, char **argv, const LaunchSettings &settings, ChangeGameFn changeGame)
{
	if (!m_hostMain)
	{
		return -1;
	}

	return m_hostMain(argc, argv, settings.defaultGameDir, 0,
		settings.allowMenuChangeGame ? changeGame : NULL);
}

void EngineLibrary::shutdownAndUnload()
{
	if (m_hostShutdown)
	{
		m_hostShutdown();
	}

	if (m_handle && m_ownsHandle)
	{
		platform::CloseLibrary(m_handle);
	}

	clear();
}

bool EngineLibrary::isLoaded() const
{
	return m_handle != NULL;
}

bool EngineLibrary::hasHostMain() const
{
	return m_hostMain != NULL;
}

bool EngineLibrary::hasHostShutdown() const
{
	return m_hostShutdown != NULL;
}

void EngineLibrary::setLoadedForTest(void *handle, HostMainFn hostMain, HostShutdownFn hostShutdown)
{
	m_handle = handle;
	m_hostMain = hostMain;
	m_hostShutdown = hostShutdown;
	m_ownsHandle = false;
}

void EngineLibrary::clear()
{
	m_handle = NULL;
	m_hostMain = NULL;
	m_hostShutdown = NULL;
	m_ownsHandle = false;
}

void EngineLibrary::formatError(char *errorBuffer, size_t errorBufferSize, const char *format, ...) const
{
	if (!errorBuffer || !errorBufferSize)
	{
		return;
	}

	va_list args;
	va_start(args, format);
	vsnprintf(errorBuffer, errorBufferSize, format, args);
	va_end(args);
	errorBuffer[errorBufferSize - 1] = '\0';
}

}
}
