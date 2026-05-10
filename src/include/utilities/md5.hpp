#ifndef XASH_UTILITIES_MD5_HPP
#define XASH_UTILITIES_MD5_HPP

#include <stdint.h>

namespace xash
{
namespace utilities
{

struct Md5Context
{
	uint32_t buf[4];
	uint32_t bits[2];
	uint32_t in[16];
};

struct Md5HexString
{
	char value[33];
};

void Md5Init(Md5Context *ctx);
void Md5Update(Md5Context *ctx, const uint8_t *buf, uint32_t len);
void Md5Final(uint8_t digest[16], Md5Context *ctx);
void Md5Transform(uint32_t buf[4], const uint32_t in[16]);
Md5HexString Md5Print(const uint8_t hash[16]);

}
}

#endif
