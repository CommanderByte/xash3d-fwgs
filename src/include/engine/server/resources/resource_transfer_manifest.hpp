#ifndef XASH_ENGINE_SERVER_RESOURCE_TRANSFER_MANIFEST_HPP
#define XASH_ENGINE_SERVER_RESOURCE_TRANSFER_MANIFEST_HPP

#include "engine/server/resources/resource_identity.hpp"
#include "engine/server/resources/server_download_policy.hpp"
#include "engine/server/resources/server_resource_catalog.hpp"
#include "engine/server/server_resource_message.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace xash
{
namespace engine
{
namespace server
{

class ResourceTransferManifest
{
public:
	void clear();
	bool empty() const;
	std::size_t size() const;

	bool append(const ResourceDescriptor &resource);
	bool append(const ResourceCatalogEntry &entry);
	std::size_t appendAll(const ResourceCatalogEntry *entries, std::size_t count);

	const ResourceDescriptor *data() const;
	const ResourceDescriptor *resourceAt(std::size_t index) const;

	int findDownloadIndex(const char *downloadName) const;
	ResourceSizeSummary sizeSummary() const;

	ServerDownloadRequest buildDownloadRequest(
		const char *requestedName,
		bool allowDownload,
		bool sendResources,
		bool sendLogos,
		const char *modelTextureName = nullptr,
		bool modelTextureAvailable = false) const;

	bool buildResourceMessageRow(
		std::size_t index,
		ResourceMessageRow &row,
		const std::uint8_t *reservedData = nullptr) const;

private:
	std::vector<ResourceDescriptor> resources_;
};

ResourceMessageRow BuildResourceTransferMessageRow(
	const ResourceDescriptor &resource,
	const std::uint8_t *reservedData = nullptr);

}
}
}

#endif
