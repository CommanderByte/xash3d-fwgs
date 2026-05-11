#include "engine/server/resources/server_hot_resource.hpp"

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

bool IsInlineModelName(const char *name)
{
	return name && name[0] == '*';
}

HotResourceFileSizeQuery SkipFileSizeQuery()
{
	HotResourceFileSizeQuery query = {};
	return query;
}

HotResourceFileSizeQuery NoFileSizeQuery()
{
	HotResourceFileSizeQuery query = {};
	query.shouldAnnounce = true;
	return query;
}

HotResourceFileSizeQuery FileSizeQuery(const std::string &path)
{
	HotResourceFileSizeQuery query = {};
	query.shouldAnnounce = true;
	query.needsFileSize = true;
	query.path = path;
	return query;
}

HotResourceAnnouncement SkipAnnouncement()
{
	HotResourceAnnouncement announcement = {};
	announcement.resource.type = ResourceType::Unknown;
	return announcement;
}

bool HotResourceNeedsFileSize(ResourceType type, const char *name)
{
	if (StringEmptyOrNull(name))
		return false;

	if (type == ResourceType::Model && IsInlineModelName(name))
		return false;

	return true;
}

}

HotResourceFileSizeQuery BuildHotResourceFileSizeQuery(
	const HotResourceRequest &request)
{
	if (StringEmptyOrNull(request.name))
		return SkipFileSizeQuery();

	if (!HotResourceNeedsFileSize(request.type, request.name))
		return NoFileSizeQuery();

	if (request.type == ResourceType::Sound)
		return FileSizeQuery(std::string(kHotResourceSoundPathPrefix) + request.name);

	return FileSizeQuery(request.name ? request.name : "");
}

HotResourceAnnouncement BuildHotResourceAnnouncement(
	const HotResourceRequest &request,
	int probedDownloadSize)
{
	if (StringEmptyOrNull(request.name))
		return SkipAnnouncement();

	HotResourceAnnouncement announcement = {};
	announcement.shouldAnnounce = true;
	announcement.resource.name = request.name;
	announcement.resource.type = request.type;
	announcement.resource.index = request.index;
	announcement.resource.downloadSize =
		HotResourceNeedsFileSize(request.type, request.name) ? probedDownloadSize : 0;
	announcement.resource.flags = request.flags;
	return announcement;
}

}
}
}
