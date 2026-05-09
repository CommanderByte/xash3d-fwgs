#ifndef XASH_LAUNCHER_PLATFORM_LIBRARY_HPP
#define XASH_LAUNCHER_PLATFORM_LIBRARY_HPP

#include <stddef.h>

namespace xash
{
namespace launcher
{
namespace platform
{

const char *DefaultEngineLibraryName();
const char *DefaultSdl2LibraryName();
bool DefaultProbeSdl2Library();

bool ProbeLibrary(const char *libraryName, char *errorBuffer, size_t errorBufferSize);
void *OpenLibrary(const char *libraryName, char *errorBuffer, size_t errorBufferSize);
void *FindLibrarySymbol(void *libraryHandle, const char *symbolName);
void CloseLibrary(void *libraryHandle);
const char *LastLibraryError();

}
}
}

#endif
