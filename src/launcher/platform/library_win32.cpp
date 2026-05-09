#include "launcher/platform/library.hpp"

#include <windows.h>

#include <stdio.h>

namespace xash
{
namespace launcher
{
namespace platform
{

const char *DefaultEngineLibraryName()
{
	return "xash.dll";
}

const char *DefaultSdl2LibraryName()
{
	return "SDL2.dll";
}

bool DefaultProbeSdl2Library()
{
	return true;
}

static void FormatError(char *errorBuffer, size_t errorBufferSize, const char *format,
	const char *name, const char *error)
{
	if (!errorBuffer || !errorBufferSize)
	{
		return;
	}

	snprintf(errorBuffer, errorBufferSize, format, name ? name : "", error ? error : "unknown error");
	errorBuffer[errorBufferSize - 1] = '\0';
}

static bool ToWidePath(const char *path, wchar_t *dst, size_t dstSize)
{
	int length;

	if (!path || !path[0] || !dst || !dstSize)
	{
		return false;
	}

	length = MultiByteToWideChar(CP_UTF8, 0, path, -1, dst, (int)dstSize);
	if (length <= 0)
	{
		length = MultiByteToWideChar(CP_ACP, 0, path, -1, dst, (int)dstSize);
	}

	return length > 0;
}

bool ProbeLibrary(const char *libraryName, char *errorBuffer, size_t errorBufferSize)
{
	wchar_t wideName[260];
	HMODULE handle;

	if (!ToWidePath(libraryName, wideName, sizeof(wideName) / sizeof(wideName[0])))
	{
		FormatError(errorBuffer, errorBufferSize, "Unable to load %s: %s",
			libraryName, "invalid library name");
		return false;
	}

	handle = LoadLibraryExW(wideName, NULL, LOAD_LIBRARY_AS_DATAFILE);
	if (!handle)
	{
		FormatError(errorBuffer, errorBufferSize, "Unable to load %s: %s",
			libraryName, LastLibraryError());
		return false;
	}

	FreeLibrary(handle);
	return true;
}

void *OpenLibrary(const char *libraryName, char *errorBuffer, size_t errorBufferSize)
{
	wchar_t wideName[260];
	HMODULE handle;

	if (!ToWidePath(libraryName, wideName, sizeof(wideName) / sizeof(wideName[0])))
	{
		FormatError(errorBuffer, errorBufferSize, "Unable to load %s: %s",
			libraryName, "invalid library name");
		return NULL;
	}

	handle = LoadLibraryW(wideName);
	if (!handle)
	{
		FormatError(errorBuffer, errorBufferSize, "Unable to load %s: %s",
			libraryName, LastLibraryError());
	}

	return handle;
}

void *FindLibrarySymbol(void *libraryHandle, const char *symbolName)
{
	if (!libraryHandle || !symbolName)
	{
		return NULL;
	}

	return (void *)GetProcAddress((HMODULE)libraryHandle, symbolName);
}

void CloseLibrary(void *libraryHandle)
{
	if (libraryHandle)
	{
		FreeLibrary((HMODULE)libraryHandle);
	}
}

const char *LastLibraryError()
{
	static char buffer[1024];

	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
		buffer, sizeof(buffer), NULL);

	return buffer;
}

}
}
}
