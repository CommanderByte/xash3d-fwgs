#ifndef XASH_UTILITIES_CHECKSUM_HPP
#define XASH_UTILITIES_CHECKSUM_HPP

#include <stdint.h>

namespace xash
{
namespace utilities
{

static const uint32_t kCrc32InitialValue = 0xFFFFFFFFu;
static const uint32_t kCrc32XorValue = 0xFFFFFFFFu;

uint32_t Crc32TableEntry(uint32_t index);
uint32_t Crc32ProcessByte(uint32_t crc, uint8_t value);
uint32_t Crc32ProcessBuffer(uint32_t crc, const void *buffer, int length);
uint32_t Crc32Final(uint32_t crc);
uint8_t Crc32BlockSequence(const uint8_t *base, int length, int sequence);

}
}

#endif
