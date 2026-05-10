#include "utilities/md5.hpp"

#include <string.h>

namespace xash
{
namespace utilities
{

namespace
{

uint32_t Swap32(uint32_t value)
{
	return ((value & 0x000000FFu) << 24) |
		((value & 0x0000FF00u) << 8) |
		((value & 0x00FF0000u) >> 8) |
		((value & 0xFF000000u) >> 24);
}

uint32_t ToLittleEndian32(uint32_t value)
{
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
	return Swap32(value);
#else
	return value;
#endif
}

void Md5SwapBlock(uint32_t *block, int count)
{
	for (int i = 0; i < count; ++i)
		block[i] = ToLittleEndian32(block[i]);
}

uint8_t HexChar(uint8_t hex)
{
	if (hex <= 0x9)
		return static_cast<uint8_t>(hex + '0');

	if (hex <= 0xF)
		return static_cast<uint8_t>(hex + '7');

	return hex;
}

void HexByteToString(uint8_t hex, char *str)
{
	*str++ = static_cast<char>(HexChar(hex >> 4));
	*str++ = static_cast<char>(HexChar(hex & 0x0F));
	*str = '\0';
}

uint32_t F1(uint32_t x, uint32_t y, uint32_t z)
{
	return z ^ (x & (y ^ z));
}

uint32_t F2(uint32_t x, uint32_t y, uint32_t z)
{
	return F1(z, x, y);
}

uint32_t F3(uint32_t x, uint32_t y, uint32_t z)
{
	return x ^ y ^ z;
}

uint32_t F4(uint32_t x, uint32_t y, uint32_t z)
{
	return y ^ (x | ~z);
}

void Md5Step(
	uint32_t (*function)(uint32_t, uint32_t, uint32_t),
	uint32_t &w,
	uint32_t x,
	uint32_t y,
	uint32_t z,
	uint32_t data,
	uint32_t shift)
{
	w += function(x, y, z) + data;
	w = (w << shift) | (w >> (32 - shift));
	w += x;
}

}

void Md5Init(Md5Context *ctx)
{
	ctx->buf[0] = 0x67452301u;
	ctx->buf[1] = 0xefcdab89u;
	ctx->buf[2] = 0x98badcfeu;
	ctx->buf[3] = 0x10325476u;

	ctx->bits[0] = 0;
	ctx->bits[1] = 0;
}

void Md5Update(Md5Context *ctx, const uint8_t *buf, uint32_t len)
{
	uint32_t t = ctx->bits[0];

	if ((ctx->bits[0] = t + (len << 3)) < t)
		++ctx->bits[1];

	ctx->bits[1] += len >> 29;
	t = (t >> 3) & 0x3f;

	if (t)
	{
		uint8_t *p = reinterpret_cast<uint8_t *>(ctx->in) + t;

		t = 64 - t;
		if (len < t)
		{
			memcpy(p, buf, len);
			return;
		}

		memcpy(p, buf, t);
		Md5SwapBlock(ctx->in, 16);
		Md5Transform(ctx->buf, ctx->in);
		buf += t;
		len -= t;
	}

	while (len >= 64)
	{
		memcpy(ctx->in, buf, 64);
		Md5SwapBlock(ctx->in, 16);
		Md5Transform(ctx->buf, ctx->in);
		buf += 64;
		len -= 64;
	}

	memcpy(ctx->in, buf, len);
}

void Md5Final(uint8_t digest[16], Md5Context *ctx)
{
	uint32_t count = (ctx->bits[0] >> 3) & 0x3F;
	uint8_t *p = reinterpret_cast<uint8_t *>(ctx->in) + count;

	*p++ = 0x80;
	count = 64 - 1 - count;

	if (count < 8)
	{
		memset(p, 0, count);
		Md5SwapBlock(ctx->in, 16);
		Md5Transform(ctx->buf, ctx->in);
		memset(ctx->in, 0, 56);
	}
	else
	{
		memset(p, 0, count - 8);
	}

	Md5SwapBlock(ctx->in, 14);
	ctx->in[14] = ctx->bits[0];
	ctx->in[15] = ctx->bits[1];

	Md5Transform(ctx->buf, ctx->in);
	Md5SwapBlock(ctx->buf, 4);
	memcpy(digest, ctx->buf, 16);
	memset(ctx, 0, sizeof(*ctx));
}

void Md5Transform(uint32_t buf[4], const uint32_t in[16])
{
	uint32_t a = buf[0];
	uint32_t b = buf[1];
	uint32_t c = buf[2];
	uint32_t d = buf[3];

	Md5Step(F1, a, b, c, d, in[0] + 0xd76aa478u, 7);
	Md5Step(F1, d, a, b, c, in[1] + 0xe8c7b756u, 12);
	Md5Step(F1, c, d, a, b, in[2] + 0x242070dbu, 17);
	Md5Step(F1, b, c, d, a, in[3] + 0xc1bdceeeu, 22);
	Md5Step(F1, a, b, c, d, in[4] + 0xf57c0fafu, 7);
	Md5Step(F1, d, a, b, c, in[5] + 0x4787c62au, 12);
	Md5Step(F1, c, d, a, b, in[6] + 0xa8304613u, 17);
	Md5Step(F1, b, c, d, a, in[7] + 0xfd469501u, 22);
	Md5Step(F1, a, b, c, d, in[8] + 0x698098d8u, 7);
	Md5Step(F1, d, a, b, c, in[9] + 0x8b44f7afu, 12);
	Md5Step(F1, c, d, a, b, in[10] + 0xffff5bb1u, 17);
	Md5Step(F1, b, c, d, a, in[11] + 0x895cd7beu, 22);
	Md5Step(F1, a, b, c, d, in[12] + 0x6b901122u, 7);
	Md5Step(F1, d, a, b, c, in[13] + 0xfd987193u, 12);
	Md5Step(F1, c, d, a, b, in[14] + 0xa679438eu, 17);
	Md5Step(F1, b, c, d, a, in[15] + 0x49b40821u, 22);

	Md5Step(F2, a, b, c, d, in[1] + 0xf61e2562u, 5);
	Md5Step(F2, d, a, b, c, in[6] + 0xc040b340u, 9);
	Md5Step(F2, c, d, a, b, in[11] + 0x265e5a51u, 14);
	Md5Step(F2, b, c, d, a, in[0] + 0xe9b6c7aau, 20);
	Md5Step(F2, a, b, c, d, in[5] + 0xd62f105du, 5);
	Md5Step(F2, d, a, b, c, in[10] + 0x02441453u, 9);
	Md5Step(F2, c, d, a, b, in[15] + 0xd8a1e681u, 14);
	Md5Step(F2, b, c, d, a, in[4] + 0xe7d3fbc8u, 20);
	Md5Step(F2, a, b, c, d, in[9] + 0x21e1cde6u, 5);
	Md5Step(F2, d, a, b, c, in[14] + 0xc33707d6u, 9);
	Md5Step(F2, c, d, a, b, in[3] + 0xf4d50d87u, 14);
	Md5Step(F2, b, c, d, a, in[8] + 0x455a14edu, 20);
	Md5Step(F2, a, b, c, d, in[13] + 0xa9e3e905u, 5);
	Md5Step(F2, d, a, b, c, in[2] + 0xfcefa3f8u, 9);
	Md5Step(F2, c, d, a, b, in[7] + 0x676f02d9u, 14);
	Md5Step(F2, b, c, d, a, in[12] + 0x8d2a4c8au, 20);

	Md5Step(F3, a, b, c, d, in[5] + 0xfffa3942u, 4);
	Md5Step(F3, d, a, b, c, in[8] + 0x8771f681u, 11);
	Md5Step(F3, c, d, a, b, in[11] + 0x6d9d6122u, 16);
	Md5Step(F3, b, c, d, a, in[14] + 0xfde5380cu, 23);
	Md5Step(F3, a, b, c, d, in[1] + 0xa4beea44u, 4);
	Md5Step(F3, d, a, b, c, in[4] + 0x4bdecfa9u, 11);
	Md5Step(F3, c, d, a, b, in[7] + 0xf6bb4b60u, 16);
	Md5Step(F3, b, c, d, a, in[10] + 0xbebfbc70u, 23);
	Md5Step(F3, a, b, c, d, in[13] + 0x289b7ec6u, 4);
	Md5Step(F3, d, a, b, c, in[0] + 0xeaa127fau, 11);
	Md5Step(F3, c, d, a, b, in[3] + 0xd4ef3085u, 16);
	Md5Step(F3, b, c, d, a, in[6] + 0x04881d05u, 23);
	Md5Step(F3, a, b, c, d, in[9] + 0xd9d4d039u, 4);
	Md5Step(F3, d, a, b, c, in[12] + 0xe6db99e5u, 11);
	Md5Step(F3, c, d, a, b, in[15] + 0x1fa27cf8u, 16);
	Md5Step(F3, b, c, d, a, in[2] + 0xc4ac5665u, 23);

	Md5Step(F4, a, b, c, d, in[0] + 0xf4292244u, 6);
	Md5Step(F4, d, a, b, c, in[7] + 0x432aff97u, 10);
	Md5Step(F4, c, d, a, b, in[14] + 0xab9423a7u, 15);
	Md5Step(F4, b, c, d, a, in[5] + 0xfc93a039u, 21);
	Md5Step(F4, a, b, c, d, in[12] + 0x655b59c3u, 6);
	Md5Step(F4, d, a, b, c, in[3] + 0x8f0ccc92u, 10);
	Md5Step(F4, c, d, a, b, in[10] + 0xffeff47du, 15);
	Md5Step(F4, b, c, d, a, in[1] + 0x85845dd1u, 21);
	Md5Step(F4, a, b, c, d, in[8] + 0x6fa87e4fu, 6);
	Md5Step(F4, d, a, b, c, in[15] + 0xfe2ce6e0u, 10);
	Md5Step(F4, c, d, a, b, in[6] + 0xa3014314u, 15);
	Md5Step(F4, b, c, d, a, in[13] + 0x4e0811a1u, 21);
	Md5Step(F4, a, b, c, d, in[4] + 0xf7537e82u, 6);
	Md5Step(F4, d, a, b, c, in[11] + 0xbd3af235u, 10);
	Md5Step(F4, c, d, a, b, in[2] + 0x2ad7d2bbu, 15);
	Md5Step(F4, b, c, d, a, in[9] + 0xeb86d391u, 21);

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}

Md5HexString Md5Print(const uint8_t hash[16])
{
	Md5HexString result = {};

	for (int i = 0; i < 16; ++i)
		HexByteToString(hash[i], &result.value[i * 2]);

	return result;
}

}
}
