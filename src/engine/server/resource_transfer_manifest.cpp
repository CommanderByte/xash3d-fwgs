#include "engine/server/resource_transfer_manifest.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool ResourceDescriptorCanEnterManifest(const ResourceDescriptor &resource)
{
	return resource.name && resource.name[0] != '\0' &&
		resource.type != ResourceType::Unknown;
}

}

void ResourceTransferManifest::clear()
{
	resources_.clear();
}

bool ResourceTransferManifest::empty() const
{
	return resources_.empty();
}

std::size_t ResourceTransferManifest::size() const
{
	return resources_.size();
}

bool ResourceTransferManifest::append(const ResourceDescriptor &resource)
{
	if (!ResourceDescriptorCanEnterManifest(resource))
		return false;

	resources_.push_back(resource);
	return true;
}

bool ResourceTransferManifest::append(const ResourceCatalogEntry &entry)
{
	if (!entry.shouldAdd)
		return false;

	return append(entry.resource);
}

std::size_t ResourceTransferManifest::appendAll(
	const ResourceCatalogEntry *entries,
	std::size_t count)
{
	std::size_t added = 0;

	if (!entries)
		return added;

	for (std::size_t i = 0; i < count; ++i)
	{
		if (append(entries[i]))
			++added;
	}

	return added;
}

const ResourceDescriptor *ResourceTransferManifest::data() const
{
	return resources_.empty() ? nullptr : resources_.data();
}

const ResourceDescriptor *ResourceTransferManifest::resourceAt(std::size_t index) const
{
	if (index >= resources_.size())
		return nullptr;

	return &resources_[index];
}

int ResourceTransferManifest::findDownloadIndex(const char *downloadName) const
{
	return FindResourceForDownloadName(data(), resources_.size(), downloadName);
}

ResourceSizeSummary ResourceTransferManifest::sizeSummary() const
{
	ResourceSizeSummary summary = EmptyResourceSizeSummary();

	for (const ResourceDescriptor &resource : resources_)
		AddResourceToSizeSummary(summary, resource);

	return summary;
}

ServerDownloadRequest ResourceTransferManifest::buildDownloadRequest(
	const char *requestedName,
	bool allowDownload,
	bool sendResources,
	bool sendLogos,
	const char *modelTextureName,
	bool modelTextureAvailable) const
{
	ServerDownloadRequest request = {};
	request.requestedName = requestedName;
	request.allowDownload = allowDownload;
	request.sendResources = sendResources;
	request.sendLogos = sendLogos;
	request.resources = data();
	request.resourceCount = resources_.size();
	request.modelTextureName = modelTextureName;
	request.modelTextureAvailable = modelTextureAvailable;
	return request;
}

bool ResourceTransferManifest::buildResourceMessageRow(
	std::size_t index,
	ResourceMessageRow &row,
	const std::uint8_t *reservedData) const
{
	const ResourceDescriptor *resource = resourceAt(index);
	if (!resource)
	{
		row = {};
		row.type = ResourceType::Unknown;
		return false;
	}

	row = BuildResourceTransferMessageRow(*resource, reservedData);
	return true;
}

ResourceMessageRow BuildResourceTransferMessageRow(
	const ResourceDescriptor &resource,
	const std::uint8_t *reservedData)
{
	ResourceMessageRow row = {};
	row.type = resource.type;
	row.name = resource.name;
	row.index = resource.index;
	row.downloadSize = resource.downloadSize;
	row.flags = resource.flags;
	row.md5Hash = resource.md5Hash;
	row.reservedData = reservedData;
	return row;
}

}
}
}
