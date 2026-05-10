#include <cstdlib>
#include <cstring>

#include "engine/server/server_hot_resource.hpp"

using namespace xash::engine::server;

static HotResourceRequest Request(
	ResourceType type,
	const char *name,
	int index,
	unsigned int flags)
{
	HotResourceRequest request = {};
	request.type = type;
	request.name = name;
	request.index = index;
	request.flags = flags;
	return request;
}

static bool TestEmptyNamesAreSkipped()
{
	const HotResourceRequest empty = Request(ResourceType::Generic, "", 1, 0);
	const HotResourceRequest nullName = Request(ResourceType::Model, nullptr, 1, 0);

	return !BuildHotResourceFileSizeQuery(empty).shouldAnnounce &&
		!BuildHotResourceAnnouncement(empty, 123).shouldAnnounce &&
		!BuildHotResourceFileSizeQuery(nullName).shouldAnnounce &&
		!BuildHotResourceAnnouncement(nullName, 123).shouldAnnounce;
}

static bool TestModelWildcardSkipsFileSizeButStillAnnounces()
{
	const HotResourceRequest request = Request(ResourceType::Model, "*12", 7, 5);
	const HotResourceFileSizeQuery query = BuildHotResourceFileSizeQuery(request);
	const HotResourceAnnouncement announcement =
		BuildHotResourceAnnouncement(request, 999);

	return query.shouldAnnounce &&
		!query.needsFileSize &&
		query.path.empty() &&
		announcement.shouldAnnounce &&
		announcement.resource.type == ResourceType::Model &&
		std::strcmp(announcement.resource.name, "*12") == 0 &&
		announcement.resource.index == 7 &&
		announcement.resource.downloadSize == 0 &&
		announcement.resource.flags == 5;
}

static bool TestModelFileSizePathUsesResourceName()
{
	const HotResourceRequest request =
		Request(ResourceType::Model, "models/w_test.mdl", 37, 1);
	const HotResourceFileSizeQuery query = BuildHotResourceFileSizeQuery(request);
	const HotResourceAnnouncement announcement =
		BuildHotResourceAnnouncement(request, 321);

	return query.shouldAnnounce &&
		query.needsFileSize &&
		query.path == "models/w_test.mdl" &&
		announcement.shouldAnnounce &&
		announcement.resource.downloadSize == 321 &&
		announcement.resource.flags == 1;
}

static bool TestSoundFileSizePathAddsLegacyPrefix()
{
	const HotResourceRequest request =
		Request(ResourceType::Sound, "weapons/pl_gun3.wav", 9, 0);
	const HotResourceFileSizeQuery query = BuildHotResourceFileSizeQuery(request);
	const HotResourceAnnouncement announcement =
		BuildHotResourceAnnouncement(request, 456);

	return query.shouldAnnounce &&
		query.needsFileSize &&
		query.path == "sound/weapons/pl_gun3.wav" &&
		announcement.shouldAnnounce &&
		announcement.resource.type == ResourceType::Sound &&
		std::strcmp(announcement.resource.name, "weapons/pl_gun3.wav") == 0 &&
		announcement.resource.index == 9 &&
		announcement.resource.downloadSize == 456;
}

static bool TestGenericAndEventUseProvidedMetadata()
{
	const HotResourceRequest generic =
		Request(ResourceType::Generic, "sprites/hud.txt", 12, 1);
	const HotResourceRequest event =
		Request(ResourceType::EventScript, "events/test.sc", 13, 7);
	const HotResourceFileSizeQuery genericQuery =
		BuildHotResourceFileSizeQuery(generic);
	const HotResourceFileSizeQuery eventQuery =
		BuildHotResourceFileSizeQuery(event);
	const HotResourceAnnouncement genericAnnouncement =
		BuildHotResourceAnnouncement(generic, -42);
	const HotResourceAnnouncement eventAnnouncement =
		BuildHotResourceAnnouncement(event, 2048);

	return genericQuery.shouldAnnounce &&
		genericQuery.needsFileSize &&
		genericQuery.path == "sprites/hud.txt" &&
		eventQuery.shouldAnnounce &&
		eventQuery.needsFileSize &&
		eventQuery.path == "events/test.sc" &&
		genericAnnouncement.shouldAnnounce &&
		genericAnnouncement.resource.downloadSize == -42 &&
		genericAnnouncement.resource.flags == 1 &&
		eventAnnouncement.shouldAnnounce &&
		eventAnnouncement.resource.downloadSize == 2048 &&
		eventAnnouncement.resource.flags == 7;
}

int main()
{
	if (!TestEmptyNamesAreSkipped() ||
		!TestModelWildcardSkipsFileSizeButStillAnnounces() ||
		!TestModelFileSizePathUsesResourceName() ||
		!TestSoundFileSizePathAddsLegacyPrefix() ||
		!TestGenericAndEventUseProvidedMetadata())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
