#include "utilities/atlas.hpp"

#include <string.h>

namespace xash
{
namespace utilities
{

void InitAtlas(Atlas *atlas, int size)
{
	memset(atlas->allocated, 0, sizeof(atlas->allocated));
	atlas->size = size;
	atlas->maxHeight = 0;
}

bool AllocateAtlasBlock(Atlas *atlas, int width, int height, int *x, int *y)
{
	int best = atlas->size;

	for (int i = 0; i <= atlas->size - width;)
	{
		int best2 = 0;
		int j;

		for (j = 0; j < width; ++j)
		{
			if (atlas->allocated[i + j] >= best)
				break;

			if (atlas->allocated[i + j] > best2)
				best2 = atlas->allocated[i + j];
		}

		if (j == width)
		{
			*x = i;
			*y = best = best2;

			if (best == 0)
				break;

			++i;
		}
		else
		{
			i += j + 1;
		}
	}

	if (best + height > atlas->size)
		return false;

	for (int i = 0; i < width; ++i)
		atlas->allocated[*x + i] = best + height;

	if (best + height > atlas->maxHeight)
		atlas->maxHeight = best + height;

	return true;
}

}
}
