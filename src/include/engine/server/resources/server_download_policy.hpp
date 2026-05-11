#ifndef XASH_ENGINE_SERVER_SERVER_DOWNLOAD_POLICY_HPP
#define XASH_ENGINE_SERVER_SERVER_DOWNLOAD_POLICY_HPP

#include "engine/server/resources/resource_identity.hpp"

#include <cstddef>
#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

enum class ServerDownloadAction
{
	Ignore,
	Reject,
	SendFile,
	SendFileWithModelTexture,
	LookupCustomLogo,
};

struct ServerDownloadRequest
{
	const char *requestedName;
	bool allowDownload;
	bool sendResources;
	bool sendLogos;
	const ResourceDescriptor *resources;
	std::size_t resourceCount;
	const char *modelTextureName;
	bool modelTextureAvailable;
};

struct ServerDownloadDecision
{
	ServerDownloadAction action;
	int resourceIndex;
	const char *fileName;
	const char *modelTextureName;
	std::uint8_t customHash[kResourceHashSize];
};

bool ServerDownloadNeedsModelTextureProbe(const ServerDownloadRequest &request);
ServerDownloadDecision DecideServerDownload(const ServerDownloadRequest &request);

}
}
}

#endif
