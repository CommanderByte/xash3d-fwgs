#include "utilities/md5.hpp"

#include <stddef.h>
#include <string.h>

extern "C"
{
#include "crclib.h"
}

static_assert(sizeof(xash::utilities::Md5Context) == sizeof(MD5Context_t), "MD5 context size must match public ABI");
static_assert(offsetof(xash::utilities::Md5Context, buf) == offsetof(MD5Context_t, buf), "MD5 buf offset must match");
static_assert(offsetof(xash::utilities::Md5Context, bits) == offsetof(MD5Context_t, bits), "MD5 bits offset must match");
static_assert(offsetof(xash::utilities::Md5Context, in) == offsetof(MD5Context_t, in), "MD5 input offset must match");

namespace
{

xash::utilities::Md5Context *ModernContext(MD5Context_t *ctx)
{
	return reinterpret_cast<xash::utilities::Md5Context *>(ctx);
}

}

extern "C" void MD5Update(MD5Context_t *ctx, const byte *buf, uint len)
{
	xash::utilities::Md5Update(ModernContext(ctx), buf, static_cast<uint32_t>(len));
}

extern "C" void MD5Final(byte digest[16], MD5Context_t *ctx)
{
	xash::utilities::Md5Final(digest, ModernContext(ctx));
}

extern "C" void MD5Transform(uint buf[4], const uint in[16])
{
	xash::utilities::Md5Transform(
		reinterpret_cast<uint32_t *>(buf),
		reinterpret_cast<const uint32_t *>(in));
}

extern "C" char *MD5_Print(byte hash[16])
{
	static char buffer[64];
	const xash::utilities::Md5HexString text = xash::utilities::Md5Print(hash);

	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, text.value, sizeof(text.value));

	return buffer;
}
