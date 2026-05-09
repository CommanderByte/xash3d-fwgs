#include <stdlib.h>
#include <string.h>

#include "utilities/registry.hpp"

using namespace xash::utilities;

struct TestEntry
{
	int value;
	const char *debugName;
};

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static bool TestOrderedRegistration()
{
	StaticRegistry<int, TestEntry, 3> registry;

	if (!registry.empty() || registry.full() || registry.count() != 0 ||
		registry.capacity() != 3)
	{
		return false;
	}

	TestEntry first = { 10, "first" };
	TestEntry second = { 20, "second" };

	if (!registry.registerEntry(7, first).ok())
		return false;

	if (!registry.registerEntry(3, second).ok())
		return false;

	const StaticRegistry<int, TestEntry, 3>::Record *record0 = registry.recordAt(0);
	const StaticRegistry<int, TestEntry, 3>::Record *record1 = registry.recordAt(1);

	if (!record0 || !record1 || registry.recordAt(2))
		return false;

	return record0->key == 7 && record0->entry.value == 10 &&
		record1->key == 3 && record1->entry.value == 20;
}

static bool TestLookupAndContains()
{
	StaticRegistry<int, TestEntry, 2> registry;
	TestEntry entry = { 42, "answer" };

	if (!registry.registerEntry(1, entry).ok())
		return false;

	const TestEntry *found = registry.find(1);
	if (!found || found->value != 42 || !ExpectString(found->debugName, "answer"))
		return false;

	if (registry.find(2) || registry.contains(2))
		return false;

	TestEntry *mutableFound = registry.find(1);
	if (!mutableFound)
		return false;

	mutableFound->value = 43;
	return registry.find(1)->value == 43 && registry.contains(1);
}

static bool TestDuplicateReject()
{
	StaticRegistry<int, TestEntry, 2> registry;
	TestEntry first = { 1, "first" };
	TestEntry replacement = { 2, "replacement" };

	if (!registry.registerEntry(5, first).ok())
		return false;

	RegistryStatus duplicate = registry.registerEntry(5, replacement);
	if (duplicate.ok() || duplicate.code != RegistryStatusCode::DuplicateKey)
		return false;

	const TestEntry *found = registry.find(5);
	return found && found->value == 1 && registry.count() == 1;
}

static bool TestDuplicateReplaceKeepsOrder()
{
	StaticRegistry<int, TestEntry, 3> registry;
	TestEntry first = { 1, "first" };
	TestEntry second = { 2, "second" };
	TestEntry replacement = { 9, "replacement" };

	if (!registry.registerEntry(5, first).ok())
		return false;

	if (!registry.registerEntry(6, second).ok())
		return false;

	if (!registry.registerEntry(5, replacement, RegistryDuplicatePolicy::Replace).ok())
		return false;

	const StaticRegistry<int, TestEntry, 3>::Record *record0 = registry.recordAt(0);
	const StaticRegistry<int, TestEntry, 3>::Record *record1 = registry.recordAt(1);

	return record0 && record1 &&
		record0->key == 5 && record0->entry.value == 9 &&
		record1->key == 6 && record1->entry.value == 2 &&
		registry.count() == 2;
}

static bool TestFullRegistry()
{
	StaticRegistry<int, TestEntry, 1> registry;
	TestEntry first = { 1, "first" };
	TestEntry second = { 2, "second" };

	if (!registry.registerEntry(1, first).ok())
		return false;

	if (!registry.full())
		return false;

	RegistryStatus full = registry.registerEntry(2, second);
	return !full.ok() && full.code == RegistryStatusCode::Full &&
		registry.count() == 1;
}

static bool TestClear()
{
	StaticRegistry<int, TestEntry, 2> registry;
	TestEntry entry = { 1, "entry" };

	if (!registry.registerEntry(1, entry).ok())
		return false;

	registry.clear();
	return registry.empty() && registry.count() == 0 && !registry.find(1);
}

static bool TestCStringKeyEqual()
{
	StaticRegistry<const char *, TestEntry, 2, CStringKeyEqual> registry;
	TestEntry pak = { 1, "pak" };

	if (!registry.registerEntry(".pak", pak).ok())
		return false;

	if (!registry.find(".pak"))
		return false;

	return !registry.find(".PAK");
}

static bool TestCaseInsensitiveCStringKeyEqual()
{
	StaticRegistry<const char *, TestEntry, 2, CaseInsensitiveCStringKeyEqual> registry;
	TestEntry pak = { 1, "pak" };
	TestEntry replacement = { 2, "pak replacement" };

	if (!registry.registerEntry(".pak", pak).ok())
		return false;

	if (!registry.find(".PAK"))
		return false;

	if (!registry.registerEntry(".PAK", replacement, RegistryDuplicatePolicy::Replace).ok())
		return false;

	return registry.count() == 1 && registry.find(".pak")->value == 2;
}

int main()
{
	if (!TestOrderedRegistration() ||
		!TestLookupAndContains() ||
		!TestDuplicateReject() ||
		!TestDuplicateReplaceKeepsOrder() ||
		!TestFullRegistry() ||
		!TestClear() ||
		!TestCStringKeyEqual() ||
		!TestCaseInsensitiveCStringKeyEqual())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
