#ifndef XASH_ENGINE_SERVER_SERVER_HOT_RESOURCE_HPP
#define XASH_ENGINE_SERVER_SERVER_HOT_RESOURCE_HPP

#include "engine/server/resources/resource_identity.hpp"

#include <string>

namespace xash
{
namespace engine
{
namespace server
{

constexpr const char *kHotResourceSoundPathPrefix = "sound/";

struct HotResourceRequest
{
	ResourceType type;
	const char *name;
	int index;
	unsigned int flags;
};

struct HotResourceFileSizeQuery
{
	bool shouldAnnounce;
	bool needsFileSize;
	std::string path;
};

struct HotResourceAnnouncement
{
	bool shouldAnnounce;
	ResourceDescriptor resource;
};

HotResourceFileSizeQuery BuildHotResourceFileSizeQuery(
	const HotResourceRequest &request);
HotResourceAnnouncement BuildHotResourceAnnouncement(
	const HotResourceRequest &request,
	int probedDownloadSize);

}
}
}

#endif
