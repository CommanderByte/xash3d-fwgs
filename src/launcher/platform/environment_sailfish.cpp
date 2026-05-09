#include "launcher/platform/environment.hpp"

#include <stdio.h>
#include <stdlib.h>

namespace xash
{
namespace launcher
{
namespace platform
{

void ApplyEnvironmentDefaults()
{
	const char *home = getenv("HOME");
	char buffer[1024];

	snprintf(buffer, sizeof(buffer), "%s/xash", home ? home : "");
	setenv("XASH3D_BASEDIR", buffer, true);
	setenv("XASH3D_RODIR", "/usr/share/harbour-xash3d-fwgs/rodir", true);
}

}
}
}
