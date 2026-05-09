#include "launcher/engine_library.hpp"

#include <stdarg.h>
#include <stdio.h>

#if XASH_POSIX
#include <dlfcn.h>
#elif XASH_WIN32
#include "port.h"
#else
#error "port me!"
#endif

namespace xash
{
namespace launcher
{

EngineLibrary::EngineLibrary()
{
	clear();
}

bool EngineLibrary::load(char *errorBuffer, size_t errorBufferSize)
{
	if (errorBuffer && errorBufferSize)
	{
		errorBuffer[0] = '\0';
	}

	EngineExportNames exports = GetEngineExportNames();

#if XASH_WIN32
	const wchar_t *sdlLibrary = Sdl2LibraryNameWide();
	const wchar_t *engineLibrary = EngineLibraryNameWide();
	HMODULE sdlHandle = LoadLibraryExW(sdlLibrary, NULL, LOAD_LIBRARY_AS_DATAFILE);

	if (!sdlHandle)
	{
		formatError(errorBuffer, errorBufferSize, "Unable to load %ls: %s", sdlLibrary, lastSystemError());
		return false;
	}

	FreeLibrary(sdlHandle);

	HMODULE engineHandle = LoadLibraryW(engineLibrary);
	if (!engineHandle)
	{
		formatError(errorBuffer, errorBufferSize, "Unable to load %ls: %s", engineLibrary, lastSystemError());
		return false;
	}

	m_handle = engineHandle;
	m_ownsHandle = true;
	m_hostMain = (HostMainFn)GetProcAddress(engineHandle, exports.hostMain);

	if (!m_hostMain)
	{
		formatError(errorBuffer, errorBufferSize, "%ls missed '%s' export: %s",
			engineLibrary, exports.hostMain, lastSystemError());
		shutdownAndUnload();
		return false;
	}

	m_hostShutdown = (HostShutdownFn)GetProcAddress(engineHandle, exports.hostShutdown);
#elif XASH_POSIX
	const char *engineLibrary = EngineLibraryName();
	void *engineHandle = dlopen(engineLibrary, RTLD_NOW);

	if (!engineHandle)
	{
		formatError(errorBuffer, errorBufferSize, "Unable to load %s: %s", engineLibrary, lastSystemError());
		return false;
	}

	m_handle = engineHandle;
	m_ownsHandle = true;
	m_hostMain = (HostMainFn)dlsym(engineHandle, exports.hostMain);

	if (!m_hostMain)
	{
		formatError(errorBuffer, errorBufferSize, "%s missed '%s' export: %s",
			engineLibrary, exports.hostMain, lastSystemError());
		shutdownAndUnload();
		return false;
	}

	m_hostShutdown = (HostShutdownFn)dlsym(engineHandle, exports.hostShutdown);
#endif

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
#if XASH_WIN32
		FreeLibrary((HMODULE)m_handle);
#elif XASH_POSIX
		dlclose(m_handle);
#endif
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

const char *EngineLibrary::lastSystemError() const
{
#if XASH_WIN32
	static char buffer[1024];

	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
		buffer, sizeof(buffer), NULL);

	return buffer;
#elif XASH_POSIX
	const char *error = dlerror();
	return error ? error : "unknown error";
#endif
}

}
}
