#ifndef XASH_UTILITIES_ATLAS_HPP
#define XASH_UTILITIES_ATLAS_HPP

namespace xash
{
namespace utilities
{

static const int kAtlasMaxSize = 1024;

struct Atlas
{
	int allocated[kAtlasMaxSize];
	int size;
	int maxHeight;
};

void InitAtlas(Atlas *atlas, int size);
bool AllocateAtlasBlock(Atlas *atlas, int width, int height, int *x, int *y);

}
}

#endif
