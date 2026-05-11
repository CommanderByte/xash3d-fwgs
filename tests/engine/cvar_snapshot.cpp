#include <cstdlib>

#include "engine/cvar_snapshot.hpp"

using namespace xash::engine;

namespace
{

bool TestMissingSnapshot()
{
	const ReadOnlyCvarSnapshot snapshot = BuildMissingReadOnlyCvarSnapshot();

	return !snapshot.exists &&
		snapshot.text.empty() &&
		snapshot.number == 0.0 &&
		snapshot.integer == 0 &&
		!snapshot.boolean;
}

bool TestNumericSnapshot()
{
	const ReadOnlyCvarSnapshot zero =
		BuildReadOnlyCvarSnapshot("0", 0.0, true);
	const ReadOnlyCvarSnapshot positive =
		BuildReadOnlyCvarSnapshot("12.75", 12.75, true);
	const ReadOnlyCvarSnapshot negative =
		BuildReadOnlyCvarSnapshot("-2.9", -2.9, true);

	return zero.exists &&
		!zero.boolean &&
		zero.integer == 0 &&
		positive.boolean &&
		positive.integer == 12 &&
		negative.boolean &&
		negative.integer == -2;
}

bool TestStringSnapshot()
{
	char text[] = "initial";
	const ReadOnlyCvarSnapshot snapshot =
		BuildReadOnlyCvarSnapshot(text, 1.0, true);

	text[0] = 'f';

	return snapshot.exists &&
		snapshot.text == "initial" &&
		snapshot.boolean;
}

bool TestNullStringIsEmpty()
{
	const ReadOnlyCvarSnapshot snapshot =
		BuildReadOnlyCvarSnapshot(nullptr, 4.0, true);

	return snapshot.exists &&
		snapshot.text.empty() &&
		snapshot.number == 4.0 &&
		snapshot.integer == 4 &&
		snapshot.boolean;
}

}

int main()
{
	if (!TestMissingSnapshot() ||
		!TestNumericSnapshot() ||
		!TestStringSnapshot() ||
		!TestNullStringIsEmpty())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
