#ifndef XASH_ENGINE_SERVER_SERVER_RESLIST_POLICY_HPP
#define XASH_ENGINE_SERVER_SERVER_RESLIST_POLICY_HPP

#include "engine/server/resources/resource_identity.hpp"

#include <string>

namespace xash
{
namespace engine
{
namespace server
{

constexpr const char *kReslistSoundPrefix = "sound/";

enum class ReslistRoute
{
	Skip,
	SoundIndex,
	GenericIndex,
};

struct ReslistTokenProbe
{
	bool safe;
	bool soundPathCandidate;
	std::string normalizedPath;
};

struct ReslistTokenDecision
{
	bool shouldIndex;
	ResourceType type;
	ReslistRoute route;
	std::string normalizedPath;
	std::string indexPath;
};

ReslistTokenProbe BuildReslistTokenProbe(const char *token);
ReslistTokenDecision BuildReslistTokenDecision(
	const ReslistTokenProbe &probe,
	bool soundFormatSupported);
ReslistTokenDecision ClassifyReslistToken(
	const char *token,
	bool soundFormatSupported);

}
}
}

#endif
