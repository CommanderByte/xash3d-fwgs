#include <cstddef>
#include <cstdlib>

#include "server/client_adapter_shared.hpp"

using namespace xash::engine::server::adapter::client;

namespace
{

enum class TestRoute
{
	Ignore = 0,
	Game = 3,
	Query = 9
};

bool TestBoolConversions()
{
	return !FromLegacyBool(0) &&
		FromLegacyBool(1) &&
		FromLegacyBool(-4) &&
		ToLegacyBool(false) == 0 &&
		ToLegacyBool(true) == 1;
}

bool TestEnumConversionKeepsOrdinal()
{
	return ToLegacyEnum(TestRoute::Ignore) == 0 &&
		ToLegacyEnum(TestRoute::Game) == 3 &&
		ToLegacyEnum(TestRoute::Query) == 9;
}

bool TestSizeConversionKeepsPositiveIntRangeOnly()
{
	return ToLegacySize(0) == 0 &&
		ToLegacySize(32) == 32 &&
		ToLegacySize(static_cast<std::size_t>(0x7fffffff)) == 0x7fffffff &&
		ToLegacySize(static_cast<std::size_t>(0x80000000ULL)) == 0;
}

}

int main()
{
	if (!TestBoolConversions() ||
		!TestEnumConversionKeepsOrdinal() ||
		!TestSizeConversionKeepsPositiveIntRangeOnly())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
