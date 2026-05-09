#include "launcher/platform/library.hpp"

#include "port.h"

#include <dlfcn.h>
#include <stdio.h>

namespace xash
{
namespace launcher
{
namespace platform
{

const char *DefaultEngineLibraryName()
{
	return OS_LIB_PREFIX "xash." OS_LIB_EXT;
}

const char *DefaultSdl2LibraryName()
{
	return "";
}

bool DefaultProbeSdl2Library()
{
	return false;
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

bool ProbeLibrary(const char *libraryName, char *errorBuffer, size_t errorBufferSize)
{
	(void)libraryName;
	(void)errorBuffer;
	(void)errorBufferSize;
	return true;
}

void *OpenLibrary(const char *libraryName, char *errorBuffer, size_t errorBufferSize)
{
	void *handle = dlopen(libraryName, RTLD_NOW);

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

	return dlsym(libraryHandle, symbolName);
}

void CloseLibrary(void *libraryHandle)
{
	if (libraryHandle)
	{
		dlclose(libraryHandle);
	}
}

const char *LastLibraryError()
{
	const char *error = dlerror();
	return error ? error : "unknown error";
}

}
}
}
