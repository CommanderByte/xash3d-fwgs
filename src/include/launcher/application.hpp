#ifndef XASH_LAUNCHER_APPLICATION_HPP
#define XASH_LAUNCHER_APPLICATION_HPP

#include "launcher/engine_library.hpp"

namespace xash
{
namespace launcher
{

int RunApplication(int argc, char **argv, EngineLibrary &engineLibrary, ChangeGameFn changeGame,
	char *errorBuffer, size_t errorBufferSize);

}
}

#endif
