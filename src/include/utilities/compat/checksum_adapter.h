#ifndef XASH_UTILITIES_COMPAT_CHECKSUM_ADAPTER_H
#define XASH_UTILITIES_COMPAT_CHECKSUM_ADAPTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const uint32_t *Xash_Crc32Table(void);

#ifdef __cplusplus
}
#endif

#endif
