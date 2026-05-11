#include <cstdlib>

#include "server/game_dll_adapter_shared.hpp"

using namespace xash::engine::server::adapter;

namespace
{

enum class TestAction
{
	First = 0,
	Second = 1,
	Explicit = 7
};

bool TestBoolConversions()
{
	return !FromLegacyBool(0) &&
		FromLegacyBool(1) &&
		FromLegacyBool(-12) &&
		ToLegacyBool(false) == 0 &&
		ToLegacyBool(true) == 1;
}

bool TestEnumConversionKeepsOrdinal()
{
	return ToLegacyEnum(TestAction::First) == 0 &&
		ToLegacyEnum(TestAction::Second) == 1 &&
		ToLegacyEnum(TestAction::Explicit) == 7;
}

}

int main()
{
	if (!TestBoolConversions() ||
		!TestEnumConversionKeepsOrdinal())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
