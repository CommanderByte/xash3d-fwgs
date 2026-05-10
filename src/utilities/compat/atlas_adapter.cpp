#include "utilities/atlas.hpp"

#include <stddef.h>

extern "C"
{
#include "atlas.h"
}

static_assert(ATLAS_MAX_SIZE == xash::utilities::kAtlasMaxSize, "atlas max size must match public ABI");
static_assert(sizeof(atlas_t) == sizeof(xash::utilities::Atlas), "atlas size must match public ABI");
static_assert(offsetof(atlas_t, allocated) == offsetof(xash::utilities::Atlas, allocated), "atlas allocation offset must match");
static_assert(offsetof(atlas_t, size) == offsetof(xash::utilities::Atlas, size), "atlas size offset must match");
static_assert(offsetof(atlas_t, max_height) == offsetof(xash::utilities::Atlas, maxHeight), "atlas max height offset must match");

namespace
{

xash::utilities::Atlas *ModernAtlas(atlas_t *atlas)
{
	return reinterpret_cast<xash::utilities::Atlas *>(atlas);
}

}

extern "C" qboolean Atlas_AllocBlock(atlas_t *atlas, int w, int h, int *x, int *y)
{
	return xash::utilities::AllocateAtlasBlock(ModernAtlas(atlas), w, h, x, y) ? true : false;
}
