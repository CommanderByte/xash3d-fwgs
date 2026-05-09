#include <stdlib.h>
#include <string.h>

#include "engine/info_string.hpp"

using namespace xash::engine;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static bool TestLookupAndValidity()
{
	InfoStringLookupBuffers buffers;
	const char *info = "\\name\\Gordon\\model\\barney\\rate\\25000";

	if (!ExpectString(InfoStringValueForKey(info, "name", buffers), "Gordon"))
		return false;

	if (!ExpectString(InfoStringValueForKey(info, "missing", buffers), ""))
		return false;

	if (!InfoStringIsValid(info))
		return false;

	if (InfoStringIsValid("\\name\\"))
		return false;

	return !InfoStringIsValid("\\name");
}

static bool TestSetRemoveAndPrefix()
{
	char info[256] = "";

	if (!InfoStringSetValueForKey(info, "name", "Gordon", sizeof(info)).ok)
		return false;

	if (!InfoStringSetValueForKey(info, "model", "barney", sizeof(info)).ok)
		return false;

	if (!ExpectString(info, "\\name\\Gordon\\model\\barney"))
		return false;

	if (!InfoStringRemoveKey(info, "name"))
		return false;

	if (!ExpectString(info, "\\model\\barney"))
		return false;

	if (!InfoStringSetValueForStarKey(info, "*sid", "123", sizeof(info)).ok)
		return false;

	InfoStringRemovePrefixedKeys(info, '*');
	return ExpectString(info, "\\model\\barney");
}

static bool TestLegacyRemovePrefixQuirk()
{
	char info[256] = "\\foo2\\bad\\foo\\good";

	if (!InfoStringRemoveKey(info, "foo"))
		return false;

	return ExpectString(info, "\\foo\\good");
}

static bool TestInvalidSetInputs()
{
	char info[256] = "\\name\\Gordon";

	if (InfoStringSetValueForKey(info, "*sid", "123", sizeof(info)).ok)
		return false;

	if (InfoStringSetValueForKey(info, "bad\\key", "1", sizeof(info)).ok)
		return false;

	if (InfoStringSetValueForKey(info, "bad", "has\\slash", sizeof(info)).ok)
		return false;

	if (InfoStringSetValueForKey(info, "bad", "has..dots", sizeof(info)).ok)
		return false;

	if (InfoStringSetValueForKey(info, "bad", "has\"quote", sizeof(info)).ok)
		return false;

	return ExpectString(info, "\\name\\Gordon");
}

static bool TestTeamLowercaseAndEmptyDelete()
{
	char info[256] = "";
	InfoStringLookupBuffers buffers;

	if (!InfoStringSetValueForKey(info, "team", "BLUE TEAM", sizeof(info)).ok)
		return false;

	if (!ExpectString(InfoStringValueForKey(info, "team", buffers), "blue team"))
		return false;

	if (!InfoStringSetValueForKey(info, "team", "", sizeof(info)).ok)
		return false;

	return ExpectString(info, "");
}

static bool TestMaxSizeAndImportantKeys()
{
	char info[80] = "";
	InfoStringLookupBuffers buffers;

	if (!InfoStringSetValueForKey(info, "junk", "abcdefghijklmnopqrstuvwxyz", sizeof(info)).ok)
		return false;

	if (!InfoStringSetValueForKey(info, "name", "GordonFreemanWithALongName", 45).ok)
		return false;

	if (!ExpectString(InfoStringValueForKey(info, "name", buffers), "GordonFreemanWithALongName"))
		return false;

	if (!ExpectString(InfoStringValueForKey(info, "junk", buffers), ""))
		return false;

	if (!InfoStringSetValueForKey(info, "payload", "value-that-does-not-fit", 10).ok)
		return false;

	return ExpectString(InfoStringValueForKey(info, "payload", buffers), "");
}

static bool TestShadowMutationSequence()
{
	char modern[128] = "";
	InfoStringLookupBuffers buffers;

	if (!InfoStringSetValueForKey(modern, "name", "Gordon", sizeof(modern)).ok)
		return false;
	if (!ExpectString(modern, "\\name\\Gordon"))
		return false;

	if (!InfoStringSetValueForKey(modern, "model", "barney", sizeof(modern)).ok)
		return false;
	if (!ExpectString(modern, "\\name\\Gordon\\model\\barney"))
		return false;

	if (!InfoStringSetValueForStarKey(modern, "*sid", "123", sizeof(modern)).ok)
		return false;
	if (!ExpectString(modern, "\\name\\Gordon\\model\\barney\\*sid\\123"))
		return false;

	if (!InfoStringRemoveKey(modern, "model"))
		return false;
	if (!ExpectString(modern, "\\name\\Gordon\\*sid\\123"))
		return false;

	InfoStringRemovePrefixedKeys(modern, '*');
	if (!ExpectString(modern, "\\name\\Gordon"))
		return false;

	if (!InfoStringSetValueForKey(modern, "name", "Alyx", sizeof(modern)).ok)
		return false;

	return ExpectString(InfoStringValueForKey(modern, "name", buffers), "Alyx") &&
		ExpectString(modern, "\\name\\Alyx");
}

int main()
{
	if (!TestLookupAndValidity() ||
		!TestSetRemoveAndPrefix() ||
		!TestLegacyRemovePrefixQuirk() ||
		!TestInvalidSetInputs() ||
		!TestTeamLowercaseAndEmptyDelete() ||
		!TestMaxSizeAndImportantKeys() ||
		!TestShadowMutationSequence())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
