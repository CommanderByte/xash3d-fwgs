#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

#include "server/resource_adapter_shared.hpp"

using namespace xash::engine::server;
using namespace xash::engine::server::adapter;

static resource_t Resource(
	const char *name,
	resourcetype_t type,
	int index,
	int downloadSize,
	unsigned char flags)
{
	resource_t resource = {};
	std::snprintf(resource.szFileName, sizeof(resource.szFileName), "%s", name);
	resource.type = type;
	resource.nIndex = index;
	resource.nDownloadSize = downloadSize;
	resource.ucFlags = flags;

	for (std::size_t i = 0; i < sizeof(resource.rgucMD5_hash); ++i)
		resource.rgucMD5_hash[i] = static_cast<unsigned char>(0xA0 + i);

	for (std::size_t i = 0; i < sizeof(resource.rguc_reserved); ++i)
		resource.rguc_reserved[i] = static_cast<unsigned char>(i + 1);

	return resource;
}

static bool TestResourceTypeMapping()
{
	return ToModernResourceType(t_sound) == ResourceType::Sound &&
		ToModernResourceType(t_skin) == ResourceType::Skin &&
		ToModernResourceType(t_model) == ResourceType::Model &&
		ToModernResourceType(t_decal) == ResourceType::Decal &&
		ToModernResourceType(t_generic) == ResourceType::Generic &&
		ToModernResourceType(t_eventscript) == ResourceType::EventScript &&
		ToModernResourceType(t_world) == ResourceType::World &&
		ToModernResourceType(static_cast<resourcetype_t>(99)) == ResourceType::Unknown &&
		ToLegacyResourceType(ResourceType::Sound) == t_sound &&
		ToLegacyResourceType(ResourceType::World) == t_world &&
		ToLegacyResourceType(ResourceType::Unknown) == t_generic &&
		ToLegacyResourceType(ResourceType::Unknown, t_decal) == t_decal;
}

static bool TestResourceDescriptorSnapshot()
{
	resource_t resources[] =
	{
		Resource("sound/weapons/pl_gun3.wav", t_sound, 4, 99, 0),
		Resource("models/player.mdl", t_model, 7, 321, RES_FATALIFMISSING),
	};

	const std::vector<ResourceDescriptor> snapshot =
		BuildResourceDescriptorSnapshot(resources, 2);
	const std::vector<ResourceDescriptor> empty =
		BuildResourceDescriptorSnapshot(nullptr, 2);
	const std::vector<ResourceDescriptor> negative =
		BuildResourceDescriptorSnapshot(resources, -1);

	if (snapshot.size() != 2 ||
		!empty.empty() ||
		!negative.empty())
	{
		return false;
	}

	return std::strcmp(snapshot[0].name, "sound/weapons/pl_gun3.wav") == 0 &&
		snapshot[0].type == ResourceType::Sound &&
		snapshot[0].index == 4 &&
		snapshot[0].downloadSize == 99 &&
		snapshot[0].md5Hash == resources[0].rgucMD5_hash &&
		std::strcmp(snapshot[1].name, "models/player.mdl") == 0 &&
		snapshot[1].type == ResourceType::Model &&
		snapshot[1].flags == RES_FATALIFMISSING;
}

static bool TestCheckedResourceIndexSnapshot()
{
	resource_t resources[] =
	{
		Resource("models/a.mdl", t_model, 0, 10, 0),
		Resource("models/b.mdl", t_model, 1, 20, RES_CHECKFILE),
		Resource("models/c.mdl", t_model, 2, 30, RES_CHECKFILE | RES_CUSTOM),
	};

	const std::vector<int> indexes =
		BuildCheckedResourceIndexSnapshot(resources, 3);
	const std::vector<int> empty =
		BuildCheckedResourceIndexSnapshot(nullptr, 3);
	const std::vector<int> negative =
		BuildCheckedResourceIndexSnapshot(resources, -1);

	return indexes.size() == 2 &&
		indexes[0] == 1 &&
		indexes[1] == 2 &&
		empty.empty() &&
		negative.empty();
}

static bool TestLegacyResourceDescriptorFields()
{
	ResourceDescriptor resource = {};
	resource.name = "models/player.mdl";
	resource.type = ResourceType::Model;
	resource.index = 7;
	resource.downloadSize = 321;
	resource.flags = RES_FATALIFMISSING | 0xF0U;

	const LegacyResourceDescriptorFields fields =
		ToLegacyResourceDescriptorFields(resource);

	resource.type = ResourceType::Unknown;
	const LegacyResourceDescriptorFields fallback =
		ToLegacyResourceDescriptorFields(resource, t_decal);

	return fields.type == t_model &&
		std::strcmp(fields.name, "models/player.mdl") == 0 &&
		fields.index == 7 &&
		fields.downloadSize == 321 &&
		fields.flags == static_cast<unsigned char>(RES_FATALIFMISSING | 0xF0U) &&
		fallback.type == t_decal;
}

static bool TestCopyStringToLegacyBuffer()
{
	char small[6] = {};
	char exact[6] = {};

	CopyStringToLegacyBuffer(small, sizeof(small), std::string("abcdef"));
	CopyStringToLegacyBuffer(exact, sizeof(exact), std::string("abcde"));
	CopyStringToLegacyBuffer(nullptr, 0, std::string("ignored"));

	return std::strcmp(small, "abcde") == 0 &&
		std::strcmp(exact, "abcde") == 0;
}

static bool TestNullResourceDescriptor()
{
	const ResourceDescriptor descriptor = ToModernResourceDescriptor(nullptr);
	const ResourceMessageRow row = ToModernResourceMessageRow(nullptr);
	const CustomizationMessage message = ToModernCustomizationMessage(nullptr, 3);

	return descriptor.type == ResourceType::Unknown &&
		descriptor.name == nullptr &&
		row.type == ResourceType::Unknown &&
		row.name == nullptr &&
		row.reservedData == nullptr &&
		message.type == ResourceType::Unknown &&
		message.playerNumber == 3 &&
		message.name == nullptr;
}

static bool TestResourceMessageRowConversion()
{
	resource_t resource =
		Resource("models/player.mdl", t_model, 7, 321, RES_FATALIFMISSING);
	const ResourceMessageRow row = ToModernResourceMessageRow(&resource);

	return row.type == ResourceType::Model &&
		std::strcmp(row.name, "models/player.mdl") == 0 &&
		row.index == 7 &&
		row.downloadSize == 321 &&
		row.flags == RES_FATALIFMISSING &&
		row.md5Hash == resource.rgucMD5_hash &&
		row.reservedData == resource.rguc_reserved;
}

static bool TestCustomizationMessageConversion()
{
	resource_t resource =
		Resource("custom.hpk", t_decal, 5, -42, RES_CUSTOM | RES_WASMISSING);
	const CustomizationMessage message =
		ToModernCustomizationMessage(&resource, 12);

	return message.playerNumber == 12 &&
		message.type == ResourceType::Decal &&
		std::strcmp(message.name, "custom.hpk") == 0 &&
		message.index == 5 &&
		message.downloadSize == -42 &&
		message.flags == (RES_CUSTOM | RES_WASMISSING) &&
		message.md5Hash == resource.rgucMD5_hash;
}

int main()
{
	if (!TestResourceTypeMapping() ||
		!TestResourceDescriptorSnapshot() ||
		!TestCheckedResourceIndexSnapshot() ||
		!TestLegacyResourceDescriptorFields() ||
		!TestCopyStringToLegacyBuffer() ||
		!TestNullResourceDescriptor() ||
		!TestResourceMessageRowConversion() ||
		!TestCustomizationMessageConversion())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
