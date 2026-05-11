#include <climits>
#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll/game_dll_string_pool_compat.hpp"

using namespace xash::engine::server;

namespace
{

static bool TestStringProcessing()
{
	const GameDllProcessedString empty = ProcessGameDllString("");
	const GameDllProcessedString nullText = ProcessGameDllString(nullptr);
	const GameDllProcessedString escapes =
		ProcessGameDllString("a\\n\\r\\t\\x\\\\");

	return empty.value.empty() &&
		empty.requiredLength == 1 &&
		nullText.value.empty() &&
		nullText.requiredLength == 1 &&
		escapes.value == std::string("a\n\r\t\\x\\\\") &&
		escapes.requiredLength == escapes.value.size() + 1;
}

static bool TestDeduplicationDefault()
{
	GameDllStringPoolCompatibilityModel pool(64, false);
	const GameDllStringAllocation first = pool.allocate("models\\nbarney");
	const GameDllStringAllocation second = pool.allocate("models\\nbarney");
	const GameDllStringPoolStats stats = pool.stats();

	return first.handle == 1 &&
		!first.duplicate &&
		second.handle == first.handle &&
		second.duplicate &&
		stats.duplicateCount == 1 &&
		stats.totalAlloc == first.value.size() + 1 &&
		std::strcmp(pool.getString(first.handle), "models\nbarney") == 0;
}

static bool TestDuplicateAllowedMode()
{
	GameDllStringPoolCompatibilityModel pool(64, true);
	const GameDllStringAllocation first = pool.allocate("abc");
	const GameDllStringAllocation second = pool.allocate("abc");
	const GameDllStringPoolStats stats = pool.stats();

	return first.handle == 1 &&
		second.handle == 5 &&
		!second.duplicate &&
		stats.duplicateCount == 0 &&
		stats.totalAlloc == 8;
}

static bool TestOverflowRewindsActiveArena()
{
	GameDllStringPoolCompatibilityModel pool(8, false);
	const GameDllStringAllocation first = pool.allocate("abc");
	const GameDllStringAllocation second = pool.allocate("de");
	const GameDllStringPoolStats stats = pool.stats();

	return first.handle == 1 &&
		second.handle == 1 &&
		second.overflowed &&
		stats.overflowCount == 1 &&
		stats.totalAlloc == 7 &&
		std::strcmp(pool.getString(second.handle), "de") == 0 &&
		pool.isValidHandle(second.handle) &&
		!pool.isValidHandle(-1) &&
		!pool.isValidHandle(99);
}

static bool TestClearAndModeSwitch()
{
	GameDllStringPoolCompatibilityModel pool(32, false);
	const GameDllStringAllocation staticString = pool.allocate("static");
	const std::size_t staticMaxAlloc = pool.stats().maxAlloc;

	pool.setDynamicMode(true);
	const GameDllStringAllocation dynamicString = pool.allocate("dynamic");
	const GameDllStringPoolStats stats = pool.stats();

	pool.clear(true);
	const GameDllStringPoolStats cleared = pool.stats();

	return staticString.handle == 1 &&
		staticMaxAlloc == 33 &&
		dynamicString.handle == 1 &&
		stats.dynamicMode &&
		stats.maxAlloc == staticMaxAlloc &&
		stats.totalAlloc == staticString.value.size() + 1 +
			dynamicString.value.size() + 1 &&
		!pool.isValidHandle(dynamicString.handle) &&
		cleared.totalAlloc == 0 &&
		cleared.maxAlloc == 0 &&
		cleared.duplicateCount == 0 &&
		cleared.overflowCount == 0;
}

static bool TestHandleZeroAndInvalidHandles()
{
	GameDllStringPoolCompatibilityModel pool(16, false);
	const GameDllStringAllocation allocation = pool.allocate("");

	return std::strcmp(pool.getString(0), "") == 0 &&
		allocation.handle == 1 &&
		std::strcmp(pool.getString(allocation.handle), "") == 0 &&
		pool.getString(-5) == nullptr &&
		pool.getString(12) == nullptr;
}

static bool TestReturnedPointersRemainStableUntilClear()
{
	GameDllStringPoolCompatibilityModel pool(128, true);
	const GameDllStringAllocation first = pool.allocate("stable");
	const char *firstPointer = pool.getString(first.handle);

	for (int i = 0; i < 10; ++i)
		pool.allocate("x");

	const bool stable =
		std::strcmp(firstPointer, "stable") == 0 &&
		std::strcmp(pool.getString(first.handle), "stable") == 0;

	pool.clear(false);

	return stable && pool.getString(first.handle) == nullptr;
}

static bool TestMakeStringAndOverrideDecisions()
{
	const GameDllMakeStringDecision existing =
		BuildGameDllMakeStringDecision(42, INT_MIN, INT_MAX);
	const GameDllMakeStringDecision high =
		BuildGameDllMakeStringDecision(
			static_cast<long long>(INT_MAX) + 1LL,
			INT_MIN,
			INT_MAX);
	const GameDllMakeStringDecision low =
		BuildGameDllMakeStringDecision(
			static_cast<long long>(INT_MIN) - 1LL,
			INT_MIN,
			INT_MAX);

	return existing.action == GameDllMakeStringAction::UseExistingOffset &&
		existing.handle == 42 &&
		high.action == GameDllMakeStringAction::AllocateFallback &&
		low.action == GameDllMakeStringAction::AllocateFallback &&
		BuildGameDllStringOverrideAction(false) ==
			GameDllStringOverrideAction::UseEnginePool &&
		BuildGameDllStringOverrideAction(true) ==
			GameDllStringOverrideAction::UsePhysicsOverride;
}

static bool TestDisplayNames()
{
	return std::strcmp(
			GameDllStringOverrideActionName(
				GameDllStringOverrideAction::UsePhysicsOverride),
			"use-physics-override") == 0 &&
		std::strcmp(
			GameDllMakeStringActionName(
				GameDllMakeStringAction::AllocateFallback),
			"allocate-fallback") == 0;
}

}

int main()
{
	if (!TestStringProcessing() ||
		!TestDeduplicationDefault() ||
		!TestDuplicateAllowedMode() ||
		!TestOverflowRewindsActiveArena() ||
		!TestClearAndModeSwitch() ||
		!TestHandleZeroAndInvalidHandles() ||
		!TestReturnedPointersRemainStableUntilClear() ||
		!TestMakeStringAndOverrideDecisions() ||
		!TestDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
