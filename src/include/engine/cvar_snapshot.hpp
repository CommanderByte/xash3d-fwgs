#ifndef XASH_ENGINE_CVAR_SNAPSHOT_HPP
#define XASH_ENGINE_CVAR_SNAPSHOT_HPP

#include <string>

namespace xash
{
namespace engine
{

struct ReadOnlyCvarSnapshot
{
	bool exists;
	std::string text;
	double number;
	int integer;
	bool boolean;
};

ReadOnlyCvarSnapshot BuildReadOnlyCvarSnapshot(
	const char *text,
	double number,
	bool exists);

ReadOnlyCvarSnapshot BuildMissingReadOnlyCvarSnapshot();

}
}

#endif
