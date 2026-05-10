#include <stdlib.h>
#include <string.h>

#include "utilities/md5.hpp"

using namespace xash::utilities;

static int TestMd5KnownDigests()
{
	Md5Context ctx;
	uint8_t digest[16];
	static const uint8_t abc[] = "abc";

	Md5Init(&ctx);
	Md5Final(digest, &ctx);
	if (strcmp(Md5Print(digest).value, "D41D8CD98F00B204E9800998ECF8427E") != 0)
		return 1;

	Md5Init(&ctx);
	Md5Update(&ctx, abc, 3);
	Md5Final(digest, &ctx);
	if (strcmp(Md5Print(digest).value, "900150983CD24FB0D6963F7D28E17F72") != 0)
		return 2;

	return 0;
}

static int TestMd5SplitUpdate()
{
	Md5Context ctx;
	uint8_t digest[16];
	static const uint8_t prefix[] = "abc";
	static const uint8_t suffix[] = "def";

	Md5Init(&ctx);
	Md5Update(&ctx, prefix, 3);
	Md5Update(&ctx, suffix, 3);
	Md5Final(digest, &ctx);

	if (strcmp(Md5Print(digest).value, "E80B5017098950FC58AAD83C8C14978E") != 0)
		return 1;

	if (ctx.buf[0] != 0 || ctx.buf[1] != 0 || ctx.bits[0] != 0 || ctx.in[0] != 0)
		return 2;

	return 0;
}

int main()
{
	int result = TestMd5KnownDigests();
	if (result != 0)
		return result;

	result = TestMd5SplitUpdate();
	if (result != 0)
		return result + 16;

	return EXIT_SUCCESS;
}
