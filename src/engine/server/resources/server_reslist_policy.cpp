#include "engine/server/resources/server_reslist_policy.hpp"

#include "utilities/path.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool StringEmptyOrNull(const char *value)
{
	return !value || value[0] == '\0';
}

void NormalizeSlashes(std::string &path)
{
	for (char &ch : path)
	{
		if (ch == '\\')
			ch = '/';
	}

	std::string compact;
	compact.reserve(path.size());

	for (std::size_t i = 0; i < path.size(); ++i)
	{
		if (path[i] == '/' && i + 1 < path.size() && path[i + 1] == '/')
			continue;

		compact.push_back(path[i]);
	}

	path.swap(compact);
}

bool StartsWith(const std::string &value, const char *prefix)
{
	const std::size_t prefixLength = std::char_traits<char>::length(prefix);
	return value.size() >= prefixLength &&
		value.compare(0, prefixLength, prefix) == 0;
}

ReslistTokenDecision SkipDecision()
{
	ReslistTokenDecision decision = {};
	decision.type = ResourceType::Unknown;
	decision.route = ReslistRoute::Skip;
	return decision;
}

}

ReslistTokenProbe BuildReslistTokenProbe(const char *token)
{
	ReslistTokenProbe probe = {};

	if (StringEmptyOrNull(token) || !IsSafeDownloadName(token))
		return probe;

	probe.safe = true;
	probe.normalizedPath = token;
	NormalizeSlashes(probe.normalizedPath);
	probe.soundPathCandidate = StartsWith(probe.normalizedPath, kReslistSoundPrefix);
	return probe;
}

ReslistTokenDecision BuildReslistTokenDecision(
	const ReslistTokenProbe &probe,
	bool soundFormatSupported)
{
	if (!probe.safe)
		return SkipDecision();

	ReslistTokenDecision decision = {};
	decision.shouldIndex = true;
	decision.normalizedPath = probe.normalizedPath;

	if (probe.soundPathCandidate && soundFormatSupported)
	{
		decision.type = ResourceType::Sound;
		decision.route = ReslistRoute::SoundIndex;
		decision.indexPath = probe.normalizedPath.substr(
			std::char_traits<char>::length(kReslistSoundPrefix));
		return decision;
	}

	decision.type = ResourceType::Generic;
	decision.route = ReslistRoute::GenericIndex;
	decision.indexPath = probe.normalizedPath;
	return decision;
}

ReslistTokenDecision ClassifyReslistToken(
	const char *token,
	bool soundFormatSupported)
{
	return BuildReslistTokenDecision(
		BuildReslistTokenProbe(token),
		soundFormatSupported);
}

}
}
}
