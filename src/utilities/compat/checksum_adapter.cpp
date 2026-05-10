#include "utilities/checksum.hpp"
#include "utilities/compat/checksum_adapter.h"

extern "C" const uint32_t *Xash_Crc32Table(void)
{
	return xash::utilities::Crc32Table();
}
