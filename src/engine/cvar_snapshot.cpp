#include "engine/cvar_snapshot.hpp"

namespace xash
{
namespace engine
{

ReadOnlyCvarSnapshot BuildReadOnlyCvarSnapshot(
	const char *text,
	double number,
	bool exists)
{
	ReadOnlyCvarSnapshot snapshot = {};
	snapshot.exists = exists;
	snapshot.text = text ? text : "";
	snapshot.number = exists ? number : 0.0;
	snapshot.integer = exists ? static_cast<int>(number) : 0;
	snapshot.boolean = exists && number != 0.0;
	return snapshot;
}

ReadOnlyCvarSnapshot BuildMissingReadOnlyCvarSnapshot()
{
	return BuildReadOnlyCvarSnapshot(nullptr, 0.0, false);
}

}
}
