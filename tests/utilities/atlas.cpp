#include <stdlib.h>

#include "utilities/atlas.hpp"

using namespace xash::utilities;

static int TestBasicAlloc()
{
	Atlas atlas;
	int x;
	int y;

	InitAtlas(&atlas, 128);

	if (!AllocateAtlasBlock(&atlas, 16, 16, &x, &y))
		return 1;

	if (x != 0 || y != 0)
		return 2;

	if (!AllocateAtlasBlock(&atlas, 16, 16, &x, &y))
		return 3;

	if (x != 16 || y != 0 || atlas.maxHeight != 16)
		return 4;

	return 0;
}

static int TestPackingAndFailure()
{
	Atlas atlas;
	int x;
	int y;

	InitAtlas(&atlas, 64);

	if (!AllocateAtlasBlock(&atlas, 32, 16, &x, &y))
		return 1;

	if (!AllocateAtlasBlock(&atlas, 32, 16, &x, &y))
		return 2;

	if (x != 32 || y != 0)
		return 3;

	if (!AllocateAtlasBlock(&atlas, 64, 16, &x, &y))
		return 4;

	if (y != 16 || atlas.maxHeight != 32)
		return 5;

	if (AllocateAtlasBlock(&atlas, 65, 1, &x, &y))
		return 6;

	if (AllocateAtlasBlock(&atlas, 1, 65, &x, &y))
		return 7;

	return 0;
}

int main()
{
	int result = TestBasicAlloc();
	if (result != 0)
		return result;

	result = TestPackingAndFailure();
	if (result != 0)
		return result + 16;

	return EXIT_SUCCESS;
}
